#include "daemon.h"
#include "messaging.h"
#include "worker_table.h"
#include "core_queue_groups.h"
#include <menabrea/log.h>
#include <menabrea/messaging.h>
#include <menabrea/exception.h>

/* Request to run the "bottom-half" of the worker deployment */
#define DAEMON_REQUEST_COMPLETE_DEPLOYMENT  ( (TMessageId) 0x01AA )

static em_status_t DaemonEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf);
static em_status_t DaemonEoStop(void * eoCtx, em_eo_t eo);
static void DaemonEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx);
static void HandleRequest(TMessage request);
static void HandleRequestCompleteDeployment(TMessage request);

static TWorkerId s_daemonWorkerId = WORKER_ID_INVALID;

void DeployDaemon(void) {

    /* Reserve the context */
    SWorkerContext * context = ReserveWorkerContext(WORKER_ID_INVALID);
    /* Daemon deployment must not fail */
    AssertTrue(context != NULL);
    context->UserInit = NULL;
    context->UserLocalInit = NULL;
    context->UserLocalExit = NULL;
    context->UserExit = NULL;
    context->WorkerBody = NULL;
    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(context->Name, "platform_daemon", sizeof(context->Name) - 1);
    context->Name[sizeof(context->Name) - 1] = '\0';
    context->CoreMask = 0b1111;
    context->Parallel = true;

    /* Create the EO - note that the daemon cannot have any local initialization
     * as we require its startup to be completely synchronous, so that it can
     * immediately service requests as other workers/EOs are starting up. */
    em_eo_t eo = em_eo_create(
        context->Name,
        DaemonEoStart,
        NULL,
        DaemonEoStop,
        NULL,
        DaemonEoReceive,
        context
    );

    /* Daemon deployment must not fail */
    AssertTrue(eo != EM_EO_UNDEF);

    /* Create the queue */
    em_queue_t queue = em_queue_create(
        context->Name,
        context->Parallel ? EM_QUEUE_TYPE_PARALLEL : EM_QUEUE_TYPE_ATOMIC,
        /* Give the daemon the highest priority - it will not handle
         * requests often and should always be given priority. */
        EM_QUEUE_PRIO_HIGHEST,
        MapCoreMaskToQueueGroup(context->CoreMask),
        NULL
    );

    /* Daemon deployment must not fail */
    AssertTrue(queue != EM_QUEUE_UNDEF);
    AssertTrue(EM_OK == em_eo_add_queue(eo, queue, 0, NULL));

    context->Eo = eo;
    context->Queue = queue;

    /* Daemon deployment must not fail */
    AssertTrue(EM_OK == em_eo_start(eo, NULL, NULL, 0, NULL));

    /* If we are here, daemon has been successfully deployed and is now ready to
     * service requests. */
    LockWorkerTableEntry(context->WorkerId);
    MarkDeploymentSuccessful(context->WorkerId);
    UnlockWorkerTableEntry(context->WorkerId);

    /* Publish the daemon's worker ID */
    s_daemonWorkerId = context->WorkerId;

    LogPrint(ELogSeverityLevel_Info, "Platform daemon deployed with worker ID: 0x%x", s_daemonWorkerId);
}

void RequestDeploymentCompletion(void) {

    /* Note that this function gets called from the worker's EO
     * final local init. The worker is almost deployed, but EM
     * will only activate its queue(s) after the last local init
     * returns. And so we cannot set the worker state to
     * EWorkerState_Active as that would invite a race condition
     * where someone tries sending a message to this worker and
     * only the final em_send() fails. We thus request the
     * platform daemon to finish the worker deployment
     * asynchronously once the state of the EO is
     * EM_EO_STATE_RUNNING. */

    TMessage request = CreateMessage(DAEMON_REQUEST_COMPLETE_DEPLOYMENT, 0);
    /* Internal message creation must not fail - we cannot back out of
     * the worker deployment now while still in the local init callback. */
    AssertTrue(request != MESSAGE_INVALID);
    SendMessage(request, s_daemonWorkerId);
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

    HandleRequest(event);
}

static void HandleRequest(TMessage request) {

    switch (GetMessageId(request)) {
    case DAEMON_REQUEST_COMPLETE_DEPLOYMENT:
        HandleRequestCompleteDeployment(request);
        break;

    default:
        LogPrint(ELogSeverityLevel_Warning, "Platform daemon received a request of unsupported type 0x%x from 0x%x", \
            GetMessageId(request), GetMessageSender(request));
    }
}

static void HandleRequestCompleteDeployment(TMessage request) {

    TWorkerId sender = GetMessageSender(request);

    /* Destroy the request message as soon as possible, we don't need it anymore */
    DestroyMessage(request);

    LogPrint(ELogSeverityLevel_Debug, "Daemon handling deployment completion request from 0x%x", \
        sender);

    LockWorkerTableEntry(sender);
    SWorkerContext * context = FetchWorkerContext(sender);
    em_eo_t eo = context->Eo;
    em_queue_t queue = context->Queue;

    /* Note that the EO cannot have been terminated yet as
     * `TerminateWorker` checks the state of the worker and the
     * state of the sender is not yet EWorkerState_Active (no
     * risk of a race condition). */

    /* This request should have been sent from the worker's EO final
     * local init - busy wait for this last init to complete if we
     * were scheduled on a different core in parallel */
    while (em_eo_get_state(eo) == EM_EO_STATE_STARTING) {
        ;
    }

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
        messagesDropped = FlushBufferedMessages(sender);

        /* Deployment complete */
        MarkDeploymentSuccessful(sender);

    } else {

        /* Termination requested during startup and now pending. Drop
         * any messages buffered to avoid unnecessary errors from EM
         * if we tried to call em_send() for each of them (or a leak
         * if we did nothing). */
        messagesDropped = DropBufferedMessages(sender);

        /* Complete deployment to have a valid state transition and
         * start teardown immediately. Note that we cannot call
         * TerminateWorker because we hold the lock. */
        MarkDeploymentSuccessful(sender);
        MarkTeardownInProgress(sender);
        /* Stop the EO - context will be released in the EO stop
         * callback */
        AssertTrue(EM_OK == em_eo_stop(context->Eo, 0, NULL));
    }

    UnlockWorkerTableEntry(sender);

    if (terminationPending) {

        /* Worker deployed and immediately terminated */
        LogPrint(ELogSeverityLevel_Info, "Worker 0x%x's termination requested while still deploying and now in progress. Dropped %d message(s)", \
            sender, messagesDropped);

    } else {

        /* Deployment successful */
        LogPrint(ELogSeverityLevel_Info, "Successfully deployed worker 0x%x (eo: %" PRI_EO ", queue: %" PRI_QUEUE ")", \
            sender, eo, queue);

        /* Log any message drops - note that an EM log should also be present for each failed em_send() call */
        if (unlikely(messagesDropped)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to send %d message(s) when flushing worker 0x%x's buffer", \
                messagesDropped, sender);
        }
    }
}
