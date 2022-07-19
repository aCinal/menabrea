#include "completion_daemon.h"
#include "worker_table.h"
#include "core_queue_groups.h"
#include "messaging.h"
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

static em_status_t DaemonEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf);
static em_status_t DaemonEoStop(void * eoCtx, em_eo_t eo);
static void DaemonEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx);
static void CompleteWorkerDeployment(TWorkerId workerId);

static em_eo_t s_daemonEo = EM_EO_UNDEF;
static em_queue_t s_daemonQueue = EM_QUEUE_UNDEF;

void DeployCompletionDaemon(void) {

    /* Create the EO - note that the daemon cannot have any local initialization
     * as we require its startup to be completely synchronous, so that it can
     * immediately service requests as other workers/EOs are starting up. */
    em_eo_t eo = em_eo_create(
        "platform_daemon",
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
        "platform_daemon",
        EM_QUEUE_TYPE_PARALLEL,
        /* Give the daemon the highest priority - it will not handle
         * events often and should always be given priority. */
        EM_QUEUE_PRIO_HIGHEST,
        MapCoreMaskToQueueGroup(0b1111),
        NULL
    );

    /* Daemon deployment must not fail */
    AssertTrue(queue != EM_QUEUE_UNDEF);
    AssertTrue(EM_OK == em_eo_add_queue(eo, queue, 0, NULL));

    /* Daemon deployment must not fail */
    AssertTrue(EM_OK == em_eo_start(eo, NULL, NULL, 0, NULL));

    /* If we are here, daemon has been successfully deployed and is now ready to
     * service requests. */
    LogPrint(ELogSeverityLevel_Info, "Successfully deployed platform daemon (eo: %" PRI_EO ", queue: %" PRI_QUEUE ")", \
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

static void CompleteWorkerDeployment(TWorkerId workerId) {

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

    int messagesDropped;
    /* Save the termination pending flag on stack so as to not use the context anymore
     * after calling em_eo_stop() (in case termination has been requested) and releasing the
     * lock, even though in the current implementation it would be perfectly safe as the
     * context does not get released until local EO stop completes on all cores - current
     * core included. */
    bool terminationPending = context->TerminationRequested;
    if (!terminationPending) {

        /* EO now running and its queues have been enabled,
         * flush any messages buffered by platform */
        messagesDropped = FlushBufferedMessages(workerId);

        /* Deployment complete */
        MarkDeploymentSuccessful(workerId);

    } else {

        /* Termination requested during startup and now pending. Drop
         * any messages buffered to avoid unnecessary errors from EM
         * if we tried to call em_send() for each of them (or a leak
         * if we did nothing). */
        messagesDropped = DropBufferedMessages(workerId);

        /* Complete deployment to have a valid state transition and
         * start teardown immediately. Note that we cannot call
         * TerminateWorker because we hold the lock. */
        MarkDeploymentSuccessful(workerId);
        MarkTeardownInProgress(workerId);
        /* Stop the EO - context will be released in the EO stop
         * callback */
        AssertTrue(EM_OK == em_eo_stop(eo, 0, NULL));
    }

    UnlockWorkerTableEntry(workerId);

    if (terminationPending) {

        /* Worker deployed and immediately terminated */
        LogPrint(ELogSeverityLevel_Info, "Worker 0x%x's termination requested while still deploying and now in progress. Dropped %d message(s)", \
            workerId, messagesDropped);

    } else {

        /* Deployment successful */
        LogPrint(ELogSeverityLevel_Info, "Successfully deployed worker 0x%x (eo: %" PRI_EO ", queue: %" PRI_QUEUE ")", \
            workerId, eo, queue);

        /* Log any message drops - note that an EM log should also be present for each failed em_send() call */
        if (unlikely(messagesDropped)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to send %d message(s) when flushing worker 0x%x's buffer", \
                messagesDropped, workerId);
        }
    }
}
