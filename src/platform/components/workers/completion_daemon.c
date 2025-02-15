#include <workers/completion_daemon.h>
#include <workers/worker_table.h>
#include <messaging/local/buffering.h>
#include <cores/queue_groups.h>
#include <menabrea/cores.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

static em_status_t DaemonEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf);
static em_status_t DaemonEoStop(void * eoCtx, em_eo_t eo);
static void DaemonEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx);
static inline void CompleteWorkerDeployment(TWorkerId workerId);

static em_eo_t s_daemonEo = EM_EO_UNDEF;
static em_queue_t s_daemonQueue = EM_QUEUE_UNDEF;

void DeployCompletionDaemon(void) {

    const char * daemonName = "completion_daemon";

    /* Create the EO - note that the daemon cannot have any local initialization
     * as we require its startup to be completely synchronous, so that it can
     * immediately service requests as other workers/EOs are starting up. */
    em_eo_t eo = em_eo_create(
        daemonName,
        DaemonEoStart,
        NULL,
        DaemonEoStop,
        NULL,
        DaemonEoReceive,
        /* Set EO context to NULL to distinguish internal platform EOs from
         * workers - 'worker' is an abstraction for application use only */
        NULL
    );

    /* Daemon deployment must not fail */
    AssertTrue(eo != EM_EO_UNDEF);

    /* Create the queue */
    em_queue_t queue = em_queue_create(
        daemonName,
        EM_QUEUE_TYPE_PARALLEL,
        /* Give the completion daemon the highest priority - it will not handle
         * events often and should always be given priority. */
        EM_QUEUE_PRIO_HIGHEST,
        MapCoreMaskToQueueGroup(GetAllCoresMask()),
        NULL
    );

    /* Daemon deployment must not fail */
    AssertTrue(queue != EM_QUEUE_UNDEF);
    AssertTrue(EM_OK == em_eo_add_queue(eo, queue, 0, NULL));

    /* Daemon deployment must not fail */
    AssertTrue(EM_OK == em_eo_start(eo, NULL, NULL, 0, NULL));

    /* If we are here, daemon has been successfully deployed and is now ready to
     * service notifications. */
    LogPrint(ELogSeverityLevel_Info, "Successfully deployed completion daemon (eo: %" PRI_EO ", queue: %" PRI_QUEUE ")", \
        eo, queue);

    s_daemonEo = eo;
    s_daemonQueue = queue;
}

em_queue_t GetCompletionDaemonQueue(void) {

    return s_daemonQueue;
}

void CompletionDaemonTeardown(void) {

    AssertTrue(EM_OK == em_eo_stop_sync(s_daemonEo));
    /* Starting from EM-ODP v1.2.3 em_eo_delete() should remove all the remaining queues and
     * delete them before deleting the actual EO */
    AssertTrue(EM_OK == em_eo_delete(s_daemonEo));

    s_daemonEo = EM_EO_UNDEF;
    s_daemonQueue = EM_QUEUE_UNDEF;
}

static em_status_t DaemonEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf) {

    /* Nothing to do */
    (void) eoCtx;
    (void) eo;
    (void) conf;

    return EM_OK;
}

static em_status_t DaemonEoStop(void * eoCtx, em_eo_t eo) {

    /* Nothing to do */
    (void) eoCtx;
    (void) eo;

    return EM_OK;
}

static void DaemonEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx) {

    (void) eoCtx;
    (void) type;
    (void) queue;
    (void) qCtx;

    /* Run the "bottom-half" of worker deployment */
    TWorkerId workerId = *(TWorkerId *) em_event_pointer(event);
    /* Free the notification event as soon as possible, we don't need it anymore */
    em_free(event);
    CompleteWorkerDeployment(workerId);
}

static inline void CompleteWorkerDeployment(TWorkerId workerId) {

    LogPrint(ELogSeverityLevel_Debug, "Daemon completing deployment of worker 0x%x", \
        workerId);

    LockWorkerTableEntry(workerId);
    SWorkerContext * context = FetchWorkerContext(workerId);
    em_eo_t eo = context->Eo;
    em_queue_t queue = context->Queue;

    /* Note that the EO cannot have been terminated yet as
     * `TerminateWorker` checks the state of the worker and the
     * state of the worker is not yet EWorkerState_Active (no
     * risk of a race condition). */

    /* The worker's EO has just completed its startup (local inits
     * completed on all cores). We only need to enable the worker
     * at platform level. */
    AssertTrue(em_eo_get_state(eo) == EM_EO_STATE_RUNNING);

    /* Copy the name to our stack for logging purposes before releasing the lock */
    char workerName[MAX_WORKER_NAME_LEN];
    (void) strcpy(workerName, context->Name);

    if (!context->TerminationRequested) {

        /* EO now running and its queues have been enabled,
         * flush any messages buffered by platform */
        int messagesDropped = FlushBufferedMessages(workerId);

        /* Deployment complete */
        MarkDeploymentSuccessful(workerId);

        UnlockWorkerTableEntry(workerId);

        /* Deployment successful */
        LogPrint(ELogSeverityLevel_Info, "Deployed '%s' (id:%x,eo:%" PRI_EO ",q:%" PRI_QUEUE ")", \
            workerName, workerId, eo, queue);

        /* Log any message drops - note that an EM log should also be present for each failed em_send() call */
        if (unlikely(messagesDropped)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to send %d message(s) when flushing worker 0x%x's buffer", \
                messagesDropped, workerId);
        }

    } else {

        /* Termination requested during startup and now pending. Drop
         * any messages buffered to avoid unnecessary errors from EM
         * if we tried to call em_send() for each of them (or a leak
         * if we did nothing). */
        int messagesDropped = DropBufferedMessages(workerId);

        /* Complete deployment to have a valid state transition and
         * start teardown immediately. Note that we cannot call
         * TerminateWorker because we hold the lock. */
        MarkDeploymentSuccessful(workerId);
        MarkTeardownInProgress(workerId);
        /* Stop the EO - context will be released in the EO stop
         * callback */
        AssertTrue(EM_OK == em_eo_stop(eo, 0, NULL));

        UnlockWorkerTableEntry(workerId);

        /* Worker deployed and immediately terminated */
        LogPrint(ELogSeverityLevel_Info, "Termination of worker '%s' (0x%x) requested during deployment and now in progress. Dropped %d message(s)", \
            workerName, workerId, messagesDropped);
    }
}
