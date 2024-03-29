#include <startup/event_dispatcher.h>
#include <startup/cpus.h>
#include <cores/queue_groups.h>
#include <exception/signal_handlers.h>
#include <input/input.h>
#include <log/log.h>
#include <log/runtime_logger.h>
#include <log/startup_logger.h>
#include <memory/memory.h>
#include <timing/setup.h>
#include <timing/timer_table.h>
#include <workers/worker_table.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <menabrea/common.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#define SYNC_DISPATCH_ROUNDS        16 * 1024
#define EXIT_CHECK_DISPATCH_ROUNDS  4 * 1024
#define DRAIN_DISPATCH_ROUNDS       64

static inline struct SStartupSharedMemory * CreatePlatformSharedMemory(SEventDispatcherConfig * config);
static inline void ActiveSync(env_atomic64_t * counter);

static bool SigchldListener(int signo, const siginfo_t * siginfo);
static bool SigintListener(int signo, const siginfo_t * siginfo);

static inline void RunMainDispatcher(void);
static inline void SetDispatcherProcessName(int physicalCore);

static inline void RunPlatformGlobalInit(void);
static inline void RunPlatformGlobalTeardown(void);

static inline void DispatcherEntryPoint(void);
static inline struct SChildren * ForkChildDispatchers(void);
static inline void ChildDispatcherInit(int physicalCore);
static inline void ChildDispatcherTeardown(void);
static inline void RunDispatchLoops(void);

static inline void DrainEvents(void);
static inline void FinalizeExit(void);
static inline void WaitForChildDispatchers(struct SChildren * children);

static inline void RunApplicationsGlobalInits(void);
static inline void RunApplicationsLocalInits(void);
static inline void RunApplicationsLocalExits(void);
static inline void RunApplicationsGlobalExits(void);

typedef struct SChildren {
    int Count;
    pid_t Pids[0];
} SChildren;

typedef struct SStartupConfig {
    /* OpenDataPlane instance handle */
    odp_instance_t OdpInstance;
    /* Loaded application libraries */
    SAppLibsSet * AppLibs;
    /* Work subsystem configuration */
    SWorkersConfig WorkersConfig;
    /* Messaging subsystem configuration */
    SMessagingConfig MessagingConfig;
    /* Memory subsystem configuration */
    SMemoryConfig MemoryConfig;
} SStartupConfig;

typedef struct SStartupSharedMemory {
    /* Flag set when dispatcher teardown should commence */
    sig_atomic_t DispatcherExitFlag;
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

static SStartupConfig s_platformConfig;
static SStartupSharedMemory * s_platformShmem = NULL;

void RunEventDispatchers(SEventDispatcherConfig * config) {

    /* Set config in static per-process data to be inherited by the child dispatchers */
    s_platformConfig.OdpInstance = config->OdpInstance;
    s_platformConfig.AppLibs = config->AppLibs;
    s_platformConfig.MemoryConfig = config->MemoryConfig;
    s_platformConfig.MessagingConfig = config->MessagingConfig;
    s_platformConfig.WorkersConfig = config->WorkersConfig;

    /* Set up shared memory */
    s_platformShmem = CreatePlatformSharedMemory(config);

    /* Install signal listeners */
    ListenForSignal(SIGCHLD, SigchldListener);
    ListenForSignal(SIGINT, SigintListener);

    RunMainDispatcher();

    env_shared_free(s_platformShmem);
}

static inline SStartupSharedMemory * CreatePlatformSharedMemory(SEventDispatcherConfig * config) {

    SStartupSharedMemory * shmPtr = \
        (SStartupSharedMemory *) env_shared_malloc(sizeof(SStartupSharedMemory));
    AssertTrue(shmPtr != NULL);
    (void) memset(shmPtr, 0, sizeof(SStartupSharedMemory));

    shmPtr->DispatcherExitFlag = 0;
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

static inline void ActiveSync(env_atomic64_t * counter) {

    /* Synchronize all cores actively, i.e. keep all of them dispatching
     * while they wait */
    int cores = em_core_count();
    env_atomic64_inc(counter);
    do {
        em_dispatch(SYNC_DISPATCH_ROUNDS);
    } while (env_atomic64_get(counter) < cores);
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
     * i.e. to all dispatchers (or to allow systemd to send SIGINT
     * to the entire control-group) */
    s_platformShmem->DispatcherExitFlag = 1;
    /* Signal handled gracefully */
    return true;
}

static inline void RunMainDispatcher(void) {

    /* Set process name */
    SetDispatcherProcessName(0);
    /* Explicitly (and likely redundantly) pin the main dispatcher to core 0 */
    PinCurrentProcessToCpu(0);

    /* Initialize EM core */
    AssertTrue(EM_OK == em_init_core());

    /* Switch to runtime logging */
    SetLoggerCallback(RuntimeLoggerCallback);

    /* Initialize the platform components */
    RunPlatformGlobalInit();
    /* Give the applications a chance to initialize before the fork */
    RunApplicationsGlobalInits();
    /* About to fork, no more init memory allocations */
    DisableInitMemoryAllocation();
    SChildren * children = ForkChildDispatchers();
    /* Common code shared by all dispatchers */
    DispatcherEntryPoint();
    /* Cancel any timers left running by the application after local exits */
    RetireAllTimers();
    /* Run global exits while other cores are still dispatching */
    RunApplicationsGlobalExits();
    /* Release the child dispatchers to complete application global exit */
    ActiveSync(&s_platformShmem->WaitForGlobalAppExitCounter);
    /* Wait for global exit to complete on all cores */
    ActiveSync(&s_platformShmem->CompleteGlobalAppExitCounter);
    /* Tear down platform components while other cores are still dispatching */
    RunPlatformGlobalTeardown();
    /* Reap child processes */
    WaitForChildDispatchers(children);

    /* Switch to back to startup logger */
    SetLoggerCallback(StartupLoggerCallback);
}

static inline void SetDispatcherProcessName(int physicalCore) {

    char procName[16];
    (void) snprintf(procName, sizeof(procName), "disp_%d", physicalCore);
    AssertTrue(0 == prctl(PR_SET_NAME, procName, 0, 0, 0));
}

static inline void RunPlatformGlobalInit(void) {

    /* Set up queue groups used by platform EOs and application 'workers' */
    SetUpQueueGroups();
    /* Initialize the workers component (application abstraction on top of EOs) */
    WorkersInit(&s_platformConfig.WorkersConfig);
    /* Initialize the internal communications component */
    MessagingInit(&s_platformConfig.MessagingConfig);
    /* Initialize the timers component */
    TimingInit();
    /* Initialize the memory pool for application use */
    MemorySetup(&s_platformConfig.MemoryConfig);
    /* Register an em_send() hook exposed by the input component */
    AssertTrue(EM_OK == em_hooks_register_send(EmApiHookSend));
}

static inline void RunPlatformGlobalTeardown(void) {

    /* Deregister an em_send() hook exposed by the input component */
    AssertTrue(EM_OK == em_hooks_unregister_send(EmApiHookSend));
    /* Tear down any workers left behind by the application */
    TerminateAllWorkers();
    /* Keep the other cores dispatching while terminating workers
     * to allow local exits to complete on all of them */
    ActiveSync(&s_platformShmem->WaitForWorkersTeardownCounter);

    /* Close up shop */
    TimingTeardown();
    MessagingTeardown();
    WorkersTeardown();
    TearDownQueueGroups();

    /* Dispatch lingering events and exit */
    FinalizeExit();

    /* Tear down application memory pool */
    MemoryTeardown();
}

static inline void DispatcherEntryPoint(void) {

    int core = em_core_id();
    LogPrint(ELogSeverityLevel_Debug, "EM core %d initialized", core);

    /* Wait for other dispatchers to initialize */
    odp_barrier_wait(&s_platformShmem->OdpStartBarrier);

    /* Do local initializaton of application libraries */
    RunApplicationsLocalInits();
    LogPrint(ELogSeverityLevel_Debug, "Local inits complete on core %d", core);

    /* Keep all cores dispatching until local init has returned in order to
     * handle sync-API function calls and to enter the main dispatch loop
     * almost at the same time */
    ActiveSync(&s_platformShmem->CompleteLocalAppInitsCounter);

    LogPrint(ELogSeverityLevel_Info, \
        "Dispatcher %d enabling input polling and entering the main dispatch loop...", \
        core);
    EnableInputPolling();

    RunDispatchLoops();

    LogPrint(ELogSeverityLevel_Info, \
        "Dispatcher %d exited the main dispatch loop. Disabling input polling...", \
        core);
    DisableInputPolling();

    /* Prevent applications from allocating extra resources in shutdown code */
    DisableTimerAllocation();
    DisableWorkerDeployment();

    RunApplicationsLocalExits();
    LogPrint(ELogSeverityLevel_Debug, "Local exit complete on core %d", core);

    /* Continue dispatching until all cores have exited the dispatch
     * loop and until local exits have returned on all cores - the
     * cores might have to react to teardown-related events such as
     * EM function completion events and notifications. */
    ActiveSync(&s_platformShmem->CompleteLocalAppExitsCounter);
}

static inline SChildren * ForkChildDispatchers(void) {

    int cores = em_core_count();
    LogPrint(ELogSeverityLevel_Info, "Spawning %d child dispatchers...", \
        cores - 1);

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
            RaiseException(EExceptionFatality_Fatal, "Failed to fork child dispatcher %d: %s (%d)", \
                i, strerror(errno), errno);
            break;

        default:
            handles->Pids[i - 1] = child;
            /* Continue forking in the parent */
            break;
        }
    }

    return handles;
}

static inline void ChildDispatcherInit(int physicalCore) {

    /* Request SIGTERM if parent dies */
    AssertTrue(0 == prctl(PR_SET_PDEATHSIG, SIGTERM));

    /* Account for a race condition when the parent has already died
     * by checking if the init process is the parent */
    if (getppid() == 1) {

        raise(SIGTERM);
    }

    /* Set process name */
    SetDispatcherProcessName(physicalCore);
    /* Migrate to own core */
    PinCurrentProcessToCpu(physicalCore);

    /* Initialize ODP locally */
    AssertTrue(0 == odp_init_local(s_platformConfig.OdpInstance, \
        ODP_THREAD_WORKER));

    /* Initialize EM core */
    AssertTrue(EM_OK == em_init_core());
}

static inline void ChildDispatcherTeardown(void) {

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

static inline void RunDispatchLoops(void) {

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

static inline void DrainEvents(void) {

    /* Drain all lingering events */
    while (em_dispatch(DRAIN_DISPATCH_ROUNDS) > 0) {
        ;
    }

    LogPrint(ELogSeverityLevel_Info, "Core %d done dispatching for good...", em_core_id());
}

static inline void FinalizeExit(void) {

    /* Do one final synchronization */
    ActiveSync(&s_platformShmem->FinalSyncCounter);
    /* Drain any lingering events */
    DrainEvents();
    /* Wait for other dispatchers to leave their dispatch loops */
    odp_barrier_wait(&s_platformShmem->OdpExitBarrier);
    /* Tear down EM on this core */
    AssertTrue(EM_OK == em_term_core());
    /* Note that ODP barriers are immediately reusable */
    odp_barrier_wait(&s_platformShmem->OdpExitBarrier);
}

static inline void WaitForChildDispatchers(SChildren * children) {

    pid_t ret;
    int status;

    /* Wait for dispatchers specifically - ODP seems to spawn its own children
     * that we do not want to wait for */
    for (int i = 0; i < children->Count; i++) {

        ret = waitpid(children->Pids[i], &status, 0);
        if (ret > 0) {

            if (WIFEXITED(status)) {

                LogPrint(ELogSeverityLevel_Info, "Child %d exited with status %d", \
                    children->Pids[i], WEXITSTATUS(status));

            } else if (WIFSIGNALED(status)) {

                LogPrint(ELogSeverityLevel_Info, "Child %d was killed by signal %d", \
                    children->Pids[i], WTERMSIG(status));

            } else {

                LogPrint(ELogSeverityLevel_Warning, "Unexpected status of child %d: %d", \
                    children->Pids[i], status);
            }

        } else {

            LogPrint(ELogSeverityLevel_Warning, "%s(): waitpid failed with errno=%d: %s", \
                __FUNCTION__, errno, strerror(errno));
        }
    }

    /* Consume the handle in this function */
    free(children);
}

static inline void RunApplicationsGlobalInits(void) {

    for (int i = 0; i < s_platformConfig.AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformConfig.AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "Running global init of %s...", \
            appLib->Name);
        appLib->GlobalInit();
    }
}

static inline void RunApplicationsLocalInits(void) {

    int core = em_core_id();
    for (int i = 0; i < s_platformConfig.AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformConfig.AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "Running local init of %s on core %d...", \
            appLib->Name, core);
        appLib->LocalInit(core);
    }
}

static inline void RunApplicationsLocalExits(void) {

    int core = em_core_id();
    for (int i = 0; i < s_platformConfig.AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformConfig.AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "Running local exit of %s on core %d...", \
            appLib->Name, core);
        appLib->LocalExit(core);
    }
}

static inline void RunApplicationsGlobalExits(void) {

    for (int i = 0; i < s_platformConfig.AppLibs->Count; i++) {

        SAppLib * appLib = &s_platformConfig.AppLibs->Libs[i];
        LogPrint(ELogSeverityLevel_Info, "Running global exit of %s...", \
            appLib->Name);
        appLib->GlobalExit();
    }
}
