
#include <menabrea/workers.h>
#include <workers/worker_table.h>
#include <workers/completion_daemon.h>
#include <cores/queue_groups.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <menabrea/common.h>
#include <event_machine.h>
#include <string.h>
#include <setjmp.h>

static em_status_t WorkerEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf);
static em_status_t WorkerEoLocalStart(void * eoCtx, em_eo_t eo);
static em_status_t WorkerEoStop(void * eoCtx, em_eo_t eo);
static em_status_t WorkerEoLocalStop(void * eoCtx, em_eo_t eo);
static void WorkerEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx);

typedef enum ECurrentEoCallback {
    ECurrentEoCallback_Start,
    ECurrentEoCallback_LocalStart,
    ECurrentEoCallback_Stop,
    ECurrentEoCallback_LocalStop,
    ECurrentEoCallback_Receive
} ECurrentEoCallback;

static ECurrentEoCallback s_currentEoCallback;
static jmp_buf s_jumpPad;

TWorkerId DeployWorker(const SWorkerConfig * config) {

    if (unlikely(config == NULL)) {

        RaiseException(EExceptionFatality_NonFatal, \
            "Passed NULL pointer for worker config");
        return WORKER_ID_INVALID;
    }

    if (unlikely(config->Name == NULL)) {

        RaiseException(EExceptionFatality_NonFatal, \
            "Passed NULL pointer for worker name");
        return WORKER_ID_INVALID;
    }

    if (unlikely(config->WorkerBody == NULL)) {

        RaiseException(EExceptionFatality_NonFatal, \
            "Passed NULL pointer for body function of worker '%s'", \
            config->Name);
        return WORKER_ID_INVALID;
    }

    LogPrint(ELogSeverityLevel_Debug, "Deploying %s worker '%s'...", \
        config->Parallel ? "parallel" : "atomic", config->Name);

    /* Reserve the context */
    SWorkerContext * context = ReserveWorkerContext(config->WorkerId);
    if (unlikely(context == NULL)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to reserve context for worker '%s'", \
            __FUNCTION__, config->Name);
        return WORKER_ID_INVALID;
    }
    context->UserInit = config->UserInit;
    context->UserLocalInit = config->UserLocalInit;
    context->UserLocalExit = config->UserLocalExit;
    context->UserExit = config->UserExit;
    context->WorkerBody = config->WorkerBody;
    /* Use the shared data field to pass the init argument to save space */
    context->SharedData = config->InitArg;
    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(context->Name, config->Name, sizeof(context->Name) - 1);
    context->Name[sizeof(context->Name) - 1] = '\0';
    context->CoreMask = config->CoreMask;
    context->Parallel = config->Parallel;

    /* Create the notification event */
    em_event_t notifEvent = em_alloc(sizeof(TWorkerId), EM_EVENT_TYPE_SW, EM_POOL_DEFAULT);
    if (unlikely(notifEvent == EM_EVENT_UNDEF)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to allocate a notification event when deploying worker '%s'", \
            __FUNCTION__, context->Name);
        ReleaseWorkerContext(context->WorkerId);
        return WORKER_ID_INVALID;
    }

    /* Set the worker ID as payload of the notification event */
    TWorkerId * notifPayload = (TWorkerId *) em_event_pointer(notifEvent);
    *notifPayload = context->WorkerId;

    /* Create the EO */
    em_eo_t eo = em_eo_create(
        context->Name,
        WorkerEoStart,
        WorkerEoLocalStart,
        WorkerEoStop,
        WorkerEoLocalStop,
        WorkerEoReceive,
        context
    );

    if (unlikely(eo == EM_EO_UNDEF)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to create execution object for worker '%s'", \
            __FUNCTION__, context->Name);
        em_free(notifEvent);
        ReleaseWorkerContext(context->WorkerId);
        return WORKER_ID_INVALID;
    }

    /* Create the queue */
    em_queue_t queue = em_queue_create(
        context->Name,
        context->Parallel ? EM_QUEUE_TYPE_PARALLEL : EM_QUEUE_TYPE_ATOMIC,
        EM_QUEUE_PRIO_NORMAL,
        MapCoreMaskToQueueGroup(context->CoreMask),
        NULL
    );

    if (unlikely(queue == EM_QUEUE_UNDEF)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to create queue for worker '%s'", \
            __FUNCTION__, context->Name);
        em_free(notifEvent);
        ReleaseWorkerContext(context->WorkerId);
        (void) em_eo_delete(eo);
        return WORKER_ID_INVALID;
    }

    AssertTrue(EM_OK == em_eo_add_queue(eo, queue, 0, NULL));

    context->Eo = eo;
    context->Queue = queue;

    /* Prepare a notification that will be delivered to the completion daemon
     * once EO initialization completes on all cores */
    em_notif_t notif = {
        .event = notifEvent,
        .queue = GetCompletionDaemonQueue(),
        .egroup = EM_EVENT_GROUP_UNDEF
    };

    /* Start the EO */
    if (unlikely(EM_OK != em_eo_start(eo, NULL, NULL, 1, &notif))) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to start the EO of worker '%s' (queue: %" PRI_QUEUE ", eo: %" PRI_EO ")", \
            __FUNCTION__, context->Name, context->Queue, context->Eo);
        em_free(notifEvent);
        ReleaseWorkerContext(context->WorkerId);
        /* Starting from EM-ODP v1.2.3 em_eo_delete() should remove all the remaining queues and
         * delete them before deleting the actual EO */
        (void) em_eo_delete(eo);
        return WORKER_ID_INVALID;
    }

    /* If we are here, then global init succeeded and note that local (per-core) inits
     * cannot fail. Sending messages to the worker's EM queue will not be possible,
     * however, until all local inits complete. The platform shall buffer any messages
     * sent from now on and when the last local init completes, the platform daemon
     * shall flush this buffer. */

    return context->WorkerId;
}

void TerminateWorker(TWorkerId workerId) {

    /* If called with WORKER_ID_INVALID, terminate current worker */
    TWorkerId realId = (workerId == WORKER_ID_INVALID) ? GetOwnWorkerId() : workerId;
    if (WorkerIdGetNode(realId) != GetOwnNodeId()) {

        RaiseException(EExceptionFatality_NonFatal, "Attempted to terminate remote worker 0x%x", \
            workerId);
        return;
    }

    LogPrint(ELogSeverityLevel_Info, "Terminating worker 0x%x...", realId);

    char workerName[MAX_WORKER_NAME_LEN];
    LockWorkerTableEntry(realId);
    SWorkerContext * context = FetchWorkerContext(realId);

    EWorkerState state = context->State;
    switch (state) {
    case EWorkerState_Active:

        /* Worker active, can terminate it immediately */

        /* Sanity-check internal consistency */
        AssertTrue(realId == context->WorkerId);

        /* Mark the worker as terminating - the context will be released in the
         * EO stop callback */
        MarkTeardownInProgress(realId);
        AssertTrue(EM_OK == em_eo_stop(context->Eo, 0, NULL));
        UnlockWorkerTableEntry(realId);
        break;

    case EWorkerState_Deploying:

        /* Worker deployment still in progress. Defer termination to the platform
         * daemon. */

        /* Sanity-check internal consistency */
        AssertTrue(realId == context->WorkerId);

        if (!context->TerminationRequested) {

            context->TerminationRequested = true;
            UnlockWorkerTableEntry(realId);

        } else {

            UnlockWorkerTableEntry(realId);
            /* Termination requested flag was already set */
            LogPrint(ELogSeverityLevel_Warning, "%s(): Worker 0x%x's termination already requested", \
                __FUNCTION__, realId);
        }
        break;

    default:

        /* Worker in invalid state */

        /* Copy the name before releasing the lock */
        (void) strcpy(workerName, context->Name);
        UnlockWorkerTableEntry(realId);

        /* Always issue a warning since this is always a failure in application design to
         * call TerminateWorker twice (or a bug if called for a bad worker ID altogether) */
        LogPrint(ELogSeverityLevel_Warning, "%s(): Worker 0x%x ('%s') in invalid state: %d", \
            __FUNCTION__, realId, workerName, state);

        if (WORKER_ID_INVALID == workerId) {

            /* That being said, when the current worker is being terminated, we handle a race
             * condition for the application, where the worker (parallel) calls TerminateWorker
             * on two cores at the same time. By handling this case we ensure that calling
             * TerminateWorker(WORKER_ID_INVALID) always interrupts execution of user code.
             * The assertion below is an internal sanity check: if the worker is executing in
             * the first place, and is in invalid state for termination, then it must be in
             * state EWorkerState_Terminating. */
            AssertTrue(EWorkerState_Terminating == state);

            if (s_currentEoCallback == ECurrentEoCallback_LocalStop || s_currentEoCallback == ECurrentEoCallback_Stop) {

                /* Tried terminating self in user exit code (local or global). Either way this
                 * is an error. So as to honour the promise that TerminateWorker(WORKER_ID_INVALID)
                 * never returns to user code, our hand is forced to raise a hard exception here. */
                RaiseException(EExceptionFatality_Fatal, \
                    "Tried terminating self (0x%x, '%s') in user exit code (current EO callback: %d)", \
                    realId, workerName, s_currentEoCallback);
            }
        }
        break;
    }

    /* If terminating current worker we want to break out of the user code and do
     * a non-local goto straight back into platform code */
    if (WORKER_ID_INVALID == workerId) {

        /* Terminating self, break out of the worker body/init immediately. */
        longjmp(s_jumpPad, 1);
    }
}

void * GetSharedData(void) {

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
    return context->SharedData;
}

void SetSharedData(void * data) {

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
    context->SharedData = data;
}

void * GetLocalData(void) {

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
    int core = em_core_id();
    /* Return data local to the current core */
    return context->LocalData[core];
}

void SetLocalData(void * data) {

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
    int core = em_core_id();
    /* Set data local to the current core */
    context->LocalData[core] = data;
}

TWorkerId GetOwnWorkerId(void) {

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
    return context->WorkerId;
}

TWorkerId FindLocalWorker(const char * name) {

    em_eo_t eo = em_eo_find(name);
    if (unlikely(EM_EO_UNDEF == eo)) {

        /* No EO found */
        return WORKER_ID_INVALID;
    }

    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(eo);
    if (unlikely(NULL == context)) {

        /* Only platform-internal EOs have no context, raise soft exception */
        RaiseException(EExceptionFatality_NonFatal, \
            "Tried looking up non-worker EO '%s'", name);
        return WORKER_ID_INVALID;
    }

    return context->WorkerId;
}

void EndAtomicContext(void) {

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);

    /* Check if parallel worker, i.e. one with a parallel queue associated with
     * them. If not (i.e. queue is of type EM_QUEUE_TYPE_ATOMIC) then we can
     * give a hint to the scheduler that it is safe to schedule another instance
     * of this worker in parallel. */
    if (!context->Parallel) {

        /* Leave atomic processing context */
        em_atomic_processing_end();
    }
}

void TerminateAllWorkers(void) {

    LogPrint(ELogSeverityLevel_Info, "Terminating lingering workers...");

    for (TWorkerId i = 0; i < MAX_WORKER_COUNT; i++) {

        SWorkerContext * context = FetchWorkerContext(i);
        if (context->State == EWorkerState_Active) {

            TerminateWorker(MakeWorkerId(GetOwnNodeId(), i));
        }
    }
}

static em_status_t WorkerEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf) {

    SWorkerContext * context = (SWorkerContext *) eoCtx;

    (void) eo;
    (void) conf;

    s_currentEoCallback = ECurrentEoCallback_Start;

    /* Recover the init argument from the shared data field */
    void * initArg = context->SharedData;
    /* No user code has been run yet, we can safely clear the shared data field */
    context->SharedData = NULL;

    if (context->UserInit) {

        /* Prepare for a non-local return in case the worker chooses to terminate */
        if (0 == setjmp(s_jumpPad)) {

            /* Call user-provided initialization function */
            int userStatus = context->UserInit(initArg);
            if (userStatus) {

                LogPrint(ELogSeverityLevel_Warning, \
                    "User's global initialization function for worker 0x%x ('%s') failed (return value: %d)", \
                    context->WorkerId, context->Name, userStatus);
                /* Return error, resources will be cleaned up in the 'DeployWorker' function */
                return EM_ERROR;
            }

        } else {

            LogPrint(ELogSeverityLevel_Debug, "Worker 0x%x ('%s') jumped back to %s", \
                context->WorkerId, context->Name, __FUNCTION__);
        }
    }

    return EM_OK;
}

static em_status_t WorkerEoLocalStart(void * eoCtx, em_eo_t eo) {

    SWorkerContext * context = (SWorkerContext *) eoCtx;

    s_currentEoCallback = ECurrentEoCallback_LocalStart;

    int core = em_core_id();
    /* Check if the worker has been deployed to the current core - only
     * run user-defined init callback on the relevant cores that will
     * host/run the worker */
    int isHostToWorker = (1 << core) & context->CoreMask;

    if (isHostToWorker && context->UserLocalInit) {

        /* Prepare for a non-local return in case the worker chooses to terminate */
        if (0 == setjmp(s_jumpPad)) {

            /* Run user-defined initialization function. Note that this will be run on
             * different cores in different processes so we rely here on the fact that the
             * address spaces look the same, i.e. the user library is loaded before
             * the fork. */
            context->UserLocalInit(core);

        } else {

            LogPrint(ELogSeverityLevel_Debug, "Worker 0x%x ('%s') jumped back to %s", \
                context->WorkerId, context->Name, __FUNCTION__);
        }
    }

    return EM_OK;
}

static em_status_t WorkerEoStop(void * eoCtx, em_eo_t eo) {

    SWorkerContext * context = (SWorkerContext *) eoCtx;

    s_currentEoCallback = ECurrentEoCallback_Stop;

    if (context->UserExit) {

        context->UserExit();
    }

    ReleaseWorkerContext(context->WorkerId);

    /* Starting from EM-ODP v1.2.3 em_eo_delete() should remove all the remaining queues and
     * delete them before deleting the actual EO */
    AssertTrue(EM_OK == em_eo_delete(eo));

    return EM_OK;
}

static em_status_t WorkerEoLocalStop(void * eoCtx, em_eo_t eo) {

    SWorkerContext * context = (SWorkerContext *) eoCtx;

    s_currentEoCallback = ECurrentEoCallback_LocalStop;

    int core = em_core_id();
    /* Check if the worker has been deployed to the current core - only
     * run user-defined exit callback on the relevant cores that used to
     * host/run the worker */
    int isHostToWorker = (1 << core) & context->CoreMask;

    if (isHostToWorker && context->UserLocalExit) {

        /* Run user-defined teardown function. Note that this will be run on
         * different cores in different processes so we rely here on the fact that the
         * address spaces look the same, i.e. the user library is loaded before
         * the fork. */
        context->UserLocalExit(core);
    }

    return EM_OK;
}

static void WorkerEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx) {

    SWorkerContext * context = (SWorkerContext *) eoCtx;

    s_currentEoCallback = ECurrentEoCallback_Receive;

    (void) type;
    (void) queue;
    (void) qCtx;

    /* Prepare for a non-local return in case the worker chooses to terminate */
    if (0 == setjmp(s_jumpPad)) {
        /* TODO: If this turns out to be too much overhead consider adding a flag in the worker context to denote that
         * the worker may never terminate on its own (we could then raise an exception in TerminateWorker on violation of
         * this promise on behalf of the user, and skip setting the landing pad here) */

        /* TODO: Add statistics/profiling here (under spinlock as the worker may be parallel) */
        AssertTrue(context->WorkerBody != NULL);
        /* Pass the event to the user-provided handler */
        context->WorkerBody(event);

    } else {

        LogPrint(ELogSeverityLevel_Debug, "Worker 0x%x ('%s') jumped back to %s", \
            context->WorkerId, context->Name, __FUNCTION__);
    }
}
