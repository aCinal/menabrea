#define _GNU_SOURCE
#include "event_dispatcher.h"
/* TODO: Resolve platform-internal dependencies more cleanly */
#include "../exception/signal_handlers.h"
#include "../work/work.h"
#include "../timing/timing.h"
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <event_machine/platform/env/environment.h>
#include <string.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#define SYNC_DISPATCH_ROUNDS          4 * 1024
#define EXIT_CHECK_DISPATCH_ROUNDS    4 * 1024
#define DRAIN_DISPATCH_ROUNDS         64

static struct SStartupSharedMemory * CreatePlatformSharedMemory(SEventDispatcherConfig * config);
static bool SigchldListener(int signo, const siginfo_t * siginfo);
static bool SigintListener(int signo, const siginfo_t * siginfo);
static void RunMainDispatcher(void);
static void DispatcherEntryPoint(void);
static void RunApplicationsGlobalInits(void);
static struct SChildren * ForkChildDispatchers(void);
static void ChildDispatcherInit(int physicalCore);
static void ChildDispatcherTeardown(void);
static void RunApplicationsLocalInits(void);
static void RunDispatchLoops(void);
static void ActiveSync(env_atomic64_t * counter);
static void DrainEvents(void);
static void FinalizeExit(void);
static void RunApplicationsLocalExits(void);
static void RunApplicationsGlobalExits(void);
static void WaitForChildDispatchers(struct SChildren * children);

typedef struct SChildren {
    int Count;
    pid_t Pids[0];
} SChildren;

typedef struct SStartupSharedMemory {
    /* OpenDataPlane instance handle */
    odp_instance_t OdpInstance;
    /* Event machine configuration */
    em_conf_t * EmConf;
    /* Flag set when dispatcher teardown should commence */
    sig_atomic_t DispatcherExitFlag;
    /* Loaded application libraries */
    SAppLibsSet * AppLibs;
    /* Work subsystem configuration */
    SWorkConfig WorkConfig;
    /* ODP barriers for EM-cores synchronization */
    odp_barrier_t OdpStartBarrier;
    odp_barrier_t OdpExitBarrier;
    /* Atomic counters for synchronizing dispatchers startup and teardown */
    env_atomic64_t CompleteLocalAppInitsCounter;   /* Wait until local application inits complete on all cores */
    env_atomic64_t CompleteLocalAppExitsCounter;   /* Wait until local application exits complete on all cores */
    env_atomic64_t WaitForGlobalAppExitCounter;    /* Wait for the main dispatcher to run application teardown */
    env_atomic64_t CompleteGlobalAppExitCounter;   /* Wait for the child dispatchers to complete application teardown */
    env_atomic64_t WaitForWorkersTeardownCounter;  /* Wait for lingering workers teardown to complete */
    env_atomic64_t FinalSyncCounter;               /* Final sync */
    /* Pad size to a multiple of cache line size */
    void * _pad[0] ENV_CACHE_LINE_ALIGNED;
} SStartupSharedMemory;

static SStartupSharedMemory * s_platformShmem = NULL;

void RunEventDispatchers(SEventDispatcherConfig * config) {

    /* Set up shared memory */
    s_platformShmem = CreatePlatformSharedMemory(config);
    LogPrint(ELogSeverityLevel_Info, "%s(): Platform memory mapped at %p", __FUNCTION__, s_platformShmem);

    /* Install signal listeners */
    AssertTrue(0 == ListenForSignal(SIGCHLD, SigchldListener));
    AssertTrue(0 == ListenForSignal(SIGINT, SigintListener));

    RunMainDispatcher();

    LogPrint(ELogSeverityLevel_Info, "%s(): Freeing platform shared memory...", __FUNCTION__);
    env_shared_free(s_platformShmem);
}

static SStartupSharedMemory * CreatePlatformSharedMemory(SEventDispatcherConfig * config) {

    SStartupSharedMemory * shmPtr = \
        (SStartupSharedMemory *) env_shared_malloc(sizeof(SStartupSharedMemory));
    AssertTrue(shmPtr != NULL);
    (void) memset(shmPtr, 0, sizeof(SStartupSharedMemory));

    shmPtr->OdpInstance = config->OdpInstance;
    shmPtr->EmConf = config->EmConf;
    shmPtr->AppLibs = config->AppLibs;
    shmPtr->DispatcherExitFlag = 0;
    shmPtr->WorkConfig = config->WorkConfig;

    /* Initialize the sync primitives */
    odp_barrier_init(&shmPtr->OdpStartBarrier, config->Cores);
    odp_barrier_init(&shmPtr->OdpExitBarrier, config->Cores);
    env_atomic64_init(&shmPtr->CompleteLocalAppInitsCounter);
    env_atomic64_init(&shmPtr->CompleteLocalAppExitsCounter);
    env_atomic64_init(&shmPtr->WaitForGlobalAppExitCounter);
    env_atomic64_init(&shmPtr->CompleteGlobalAppExitCounter);
    env_atomic64_init(&shmPtr->WaitForWorkersTeardownCounter);
    env_atomic64_init(&shmPtr->FinalSyncCounter);

    return shmPtr;
}

static bool SigchldListener(int signo, const siginfo_t * siginfo) {

    pid_t child;
    int status;

    (void) signo;

    /* Check if child process termination has been requested as
     * part of regular teardown. If so, simply return from the
     * handler. Note that it is impossible for s_platformShmem
     * to be NULL when SIGCHLD is received and handled here. */
    if (s_platformShmem->DispatcherExitFlag) {
        /* Indicate that the signal has been handled - do not restore
         * default behaviour - shouldn't make a difference anyway. */
        return true;

    } else {

        /* Child died unexpectedly */

        LogPrint(ELogSeverityLevel_Error, "Child process %d died unexpectedly", \
            siginfo->si_pid);

        /* Do non-blocking waits until no more dead children are found */
        do {
            child = waitpid(-1, &status, WNOHANG);
        } while (child > 0);

        if (child == -1 && errno != ECHILD) {

            LogPrint(ELogSeverityLevel_Warning, "%s(): waitpid failed with errno=%d: %s", \
                __FUNCTION__, errno, strerror(errno));
        }

        /* Exit the parent process, which should trigger SIGTERM in the remaining children */
        exit(EXIT_FAILURE);

        /* Will not get here */
        return false;
    }
}

static bool SigintListener(int signo, const siginfo_t * siginfo) {

    (void) signo;
    (void) siginfo;

    /* Use idempotent implementation to allow sending SIGINT from
     * the terminal during development to all foreground processes,
     * i.e. to all dispatchers */
    s_platformShmem->DispatcherExitFlag = 1;
    /* Signal handled gracefully */
    return true;
}

static void RunMainDispatcher(void) {

    const char * procName = "disp_0";
    LogPrint(ELogSeverityLevel_Info, "%s(): Changing process name to %s", \
        __FUNCTION__, procName);
    AssertTrue(0 == prctl(PR_SET_NAME, procName, 0, 0, 0));

    /* Initialize EM core */
    AssertTrue(EM_OK == em_init_core());

    WorkInit(&s_platformShmem->WorkConfig);
    TimingInit();

    RunApplicationsGlobalInits();
    SChildren * children = ForkChildDispatchers();

    /* Common code shared by all dispatchers */
    DispatcherEntryPoint();

    /* Run global exits while other cores are still dispatching */
    RunApplicationsGlobalExits();

    /* Release the child dispatchers to complete application global exit */
    ActiveSync(&s_platformShmem->WaitForGlobalAppExitCounter);
    /* Wait for global exit to complete on all cores */
    ActiveSync(&s_platformShmem->CompleteGlobalAppExitCounter);


    /* Tear down any workers left behind by the application */
    TerminateAllWorkers();

    ActiveSync(&s_platformShmem->WaitForWorkersTeardownCounter);

    /* Close up shop - note that timers may continue to fire after
     * workers have been terminated so a few send failures are
     * expected here */
    TimingTeardown();
    WorkTeardown();

    /* Dispatch lingering events and exit */
    FinalizeExit();

    /* Reap child processes */
    WaitForChildDispatchers(children);
}

static void DispatcherEntryPoint(void) {

    int core = em_core_id();

    LogPrint(ELogSeverityLevel_Debug, "%s(): EM core %d initialized", __FUNCTION__, core);

    /* Wait for other dispatchers to initialize */
    odp_barrier_wait(&s_platformShmem->OdpStartBarrier);

    /* Do local initializaton of application libraries */
    RunApplicationsLocalInits();

    LogPrint(ELogSeverityLevel_Debug, "%s(): Local inits complete on core %d", __FUNCTION__, core);

    /* Keep all cores dispatching until local init has returned in order to
     * handle sync-API function calls and to enter the main dispatch loop
     * almost at the same time */
    ActiveSync(&s_platformShmem->CompleteLocalAppInitsCounter);

    LogPrint(ELogSeverityLevel_Debug, "%s(): Dispatcher %d entering the main dispatch loop...", \
        __FUNCTION__, core);
    RunDispatchLoops();
    LogPrint(ELogSeverityLevel_Debug, "%s(): Dispatcher %d exited the main dispatch loop", \
        __FUNCTION__, core);

    RunApplicationsLocalExits();

    LogPrint(ELogSeverityLevel_Debug, "%s(): Local exit complete on core %d", __FUNCTION__, core);

    /* Continue dispatching until all cores have exited the dispatch
     * loop and until local exits have returned on all cores - the
     * cores might have to react to teardown-related events such as
     * EM function completion events and notifications. */
    ActiveSync(&s_platformShmem->CompleteLocalAppExitsCounter);
}

static void RunApplicationsGlobalInits(void) {

    for (int i = 0; i < s_platformShmem->AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformShmem->AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "%s(): Running global init of %s...", \
            __FUNCTION__, appLib->Name);
        appLib->GlobalInit();
    }
}

static SChildren * ForkChildDispatchers(void) {

    int cores = em_core_count();
    LogPrint(ELogSeverityLevel_Info, "%s(): Spawning %d child dispatchers...", \
        __FUNCTION__, cores - 1);

    SChildren * handles = malloc(sizeof(SChildren) + (cores - 1) * sizeof(pid_t));
    AssertTrue(handles != NULL);
    handles->Count = cores - 1;

    for (int i = 1; i < cores; i++) {

        pid_t child = fork();
        switch (child) {
        case 0:
            /* In child process */
            ChildDispatcherInit(i);
            DispatcherEntryPoint();
            ChildDispatcherTeardown();
            break;

        case -1:
            RaiseException(EExceptionFatality_Fatal, errno, "Failed to fork child dispatcher %d: %s", \
                i, strerror(errno));
            break;

        default:
            handles->Pids[i - 1] = child;
            /* Continue forking in the parent */
            break;
        }
    }

    return handles;
}

static void ChildDispatcherInit(int physicalCore) {

    char procName[16];
    /* Request SIGTERM if parent dies */
    AssertTrue(0 == prctl(PR_SET_PDEATHSIG, SIGTERM));

    /* Account for a race condition when the parent has already died
     * by checking if the init process is the parent */
    if (getppid() == 1) {

        raise(SIGTERM);
    }

    /* Set process name */
    (void) snprintf(procName, sizeof(procName), "disp_%1d", physicalCore);
    AssertTrue(0 == prctl(PR_SET_NAME, procName, 0, 0, 0));

    /* Migrate to own core */
    cpu_set_t cpuMask;
    CPU_ZERO(&cpuMask);
    CPU_SET(physicalCore, &cpuMask);
    AssertTrue(0 == sched_setaffinity(0, sizeof(cpu_set_t), &cpuMask));

    /* Initialize ODP locally (note that the main dispatcher) */
    AssertTrue(0 == odp_init_local(s_platformShmem->OdpInstance, \
        ODP_THREAD_WORKER));

    /* Initialize EM core */
    AssertTrue(EM_OK == em_init_core());
}

static void ChildDispatcherTeardown(void) {

    /* Wait for application global exit to finish */
    ActiveSync(&s_platformShmem->WaitForGlobalAppExitCounter);
    /* Wait for global exit to complete on all cores */
    ActiveSync(&s_platformShmem->CompleteGlobalAppExitCounter);
    /* Wait for lingering workers teardown to complete on all cores */
    ActiveSync(&s_platformShmem->WaitForWorkersTeardownCounter);

    /* Dispatch lingering events and exit */
    FinalizeExit();

    /* Tear down ODP locally. Note that odp_term_local() returns 0 or 1 on
     * success, depending on whether or not more ODP threads still exist */
    AssertTrue(0 <= odp_term_local());

    /* Terminate the child */
    exit(EXIT_SUCCESS);
}

static void RunApplicationsLocalInits(void) {

    int core = em_core_id();
    for (int i = 0; i < s_platformShmem->AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformShmem->AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "%s(): Running local init of %s on core %d...", \
            __FUNCTION__, appLib->Name, core);
        appLib->LocalInit(core);
    }
}

static void RunDispatchLoops(void) {

    while (!s_platformShmem->DispatcherExitFlag) {

        /* Dispatch forever in chunks of EXIT_CHECK_DISPATCH_ROUNDS
         * - check if 'DispatcherExitFlag' has been set by SIGINT
         * in between chunks */
        em_dispatch(EXIT_CHECK_DISPATCH_ROUNDS);
    }

    /* Allow apps one more round with 'DispatcherExitFlag' set to
     * flush events from the queues, etc. */
    em_dispatch(EXIT_CHECK_DISPATCH_ROUNDS);
}

static void ActiveSync(env_atomic64_t * counter) {

    /* Synchronize all cores actively, i.e. keep all of them dispatching
     * while they wait */
    int cores = em_core_count();
    env_atomic64_inc(counter);
    do {
        em_dispatch(SYNC_DISPATCH_ROUNDS);
    } while (env_atomic64_get(counter) < cores);
}

static void DrainEvents(void) {

    /* Drain all lingering events */
    while (em_dispatch(DRAIN_DISPATCH_ROUNDS) > 0) {
        ;
    }
}

static void FinalizeExit(void) {

    int core = em_core_id();

    /* Do one final synchronization */
    ActiveSync(&s_platformShmem->FinalSyncCounter);

    /* Drain any lingering events */
    DrainEvents();

    LogPrint(ELogSeverityLevel_Debug, "%s(): Core %d done dispatching for good...", \
        __FUNCTION__, core);

    /* Wait for other dispatchers to leave their dispatch loops */
    odp_barrier_wait(&s_platformShmem->OdpExitBarrier);

    AssertTrue(EM_OK == em_term_core());

    /* Note that ODP barriers are immediately reusable */
    odp_barrier_wait(&s_platformShmem->OdpExitBarrier);
}

static void RunApplicationsLocalExits(void) {

    int core = em_core_id();
    for (int i = 0; i < s_platformShmem->AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformShmem->AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "%s(): Running local exit of %s on core %d...", \
            __FUNCTION__, appLib->Name, core);
        appLib->LocalExit(core);
    }
}

static void RunApplicationsGlobalExits(void) {

    for (int i = 0; i < s_platformShmem->AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformShmem->AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "%s(): Running global exit of %s...", \
            __FUNCTION__, appLib->Name);
        appLib->GlobalExit();
    }
}

static void WaitForChildDispatchers(SChildren * children) {

    pid_t ret;
    int status;

    /* Wait for dispatchers specifically - ODP seems to spawn its own children
     * that we do not want to wait for */
    for (int i = 0; i < children->Count; i++) {

        ret = waitpid(children->Pids[i], &status, 0);
        if (ret > 0) {

            if (WIFEXITED(status)) {

                LogPrint(ELogSeverityLevel_Info, "%s(): Child %d exited with status %d", \
                    __FUNCTION__, children->Pids[i], WEXITSTATUS(status));

            } else if (WIFSIGNALED(status)) {

                LogPrint(ELogSeverityLevel_Info, "%s(): Child %d was killed by signal %d", \
                    __FUNCTION__, children->Pids[i], WTERMSIG(status));

            } else {

                LogPrint(ELogSeverityLevel_Warning, "%s(): Unexpected status of child %d: %d", \
                    __FUNCTION__, children->Pids[i], status);
            }

        } else {

            LogPrint(ELogSeverityLevel_Warning, "%s(): waitpid failed with errno=%d: %s", \
                __FUNCTION__, errno, strerror(errno));
        }
    }

    /* Consume the handle in this function */
    free(children);
}
