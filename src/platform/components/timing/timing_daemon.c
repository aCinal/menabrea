#include "timing_daemon.h"
#include "timer_table.h"
#include "../work/worker_table.h"
#include "../work/core_queue_groups.h"
#include <menabrea/timing.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>

static em_status_t DaemonEoStart(void * eoCtx, em_eo_t eo, const em_eo_conf_t * conf);
static em_status_t DaemonEoStop(void * eoCtx, em_eo_t eo);
static void DaemonEoReceive(void * eoCtx, em_event_t event, em_event_type_t type, em_queue_t queue, void * qCtx);
static inline void HandleTimeoutEvent(em_event_t event);

static em_eo_t s_daemonEo = EM_EO_UNDEF;
static em_queue_t s_daemonQueue = EM_QUEUE_UNDEF;

void DeployTimingDaemon(void) {

    /* Create the EO - note that the daemon cannot have any local initialization
     * as we require its startup to be completely synchronous, so that it can
     * immediately service timeout */
    em_eo_t eo = em_eo_create(
        "timing_daemon",
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
        "timing_daemon",
        EM_QUEUE_TYPE_PARALLEL,
        /* Give the timing daemon normal priority so that timeout events
         * handling here does not lead to starvation of actual application
         * handlers */
        EM_QUEUE_PRIO_NORMAL,
        /* Run the timing daemon on isolated cores only */
        MapCoreMaskToQueueGroup(GetIsolatedCoresMask()),
        NULL
    );

    /* Daemon deployment must not fail */
    AssertTrue(queue != EM_QUEUE_UNDEF);
    AssertTrue(EM_OK == em_eo_add_queue(eo, queue, 0, NULL));

    /* Daemon deployment must not fail */
    AssertTrue(EM_OK == em_eo_start(eo, NULL, NULL, 0, NULL));

    /* If we are here, daemon has been successfully deployed and is now ready to
     * service timeouts. */
    LogPrint(ELogSeverityLevel_Info, "Successfully deployed timing daemon (eo: %" PRI_EO ", queue: %" PRI_QUEUE ")", \
        eo, queue);

    s_daemonEo = eo;
    s_daemonQueue = queue;
}

em_queue_t GetTimingDaemonQueue(void) {

    return s_daemonQueue;
}

void TimingDaemonTeardown(void) {

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

    HandleTimeoutEvent(event);
}

static inline void HandleTimeoutEvent(em_event_t event) {

    TTimerId timerId = *(TTimerId *) em_event_pointer(event);

    LogPrint(ELogSeverityLevel_Debug, "%s(): Handling timeout event for timer 0x%x...", \
        __FUNCTION__, timerId);

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    em_event_t currentEvent = EM_EVENT_UNDEF;
    ETimerState state = context->State;
    switch (state) {
    case ETimerState_Armed:
        /* Timer fired normally */
        if (context->Period > 0) {

            /* Periodic timer */

            /* Send a copy of the original message */
            TMessage messageCopy = CopyMessage(context->Message);
            if (likely(messageCopy != MESSAGE_INVALID)) {

                SendMessage(messageCopy, context->Receiver);
                /* Acknowledge the timer - reuse the timeout event */
                AssertTrue(EM_OK == em_tmo_ack(context->Tmo, event));
                UnlockTimerTableEntry(timerId);

            } else {

                /* Save the message metadata on stack before releasing the context lock */
                TMessageId msgId = GetMessageId(context->Message);
                TWorkerId receiver = context->Receiver;
                /* Acknowledge the timer - reuse the timeout event */
                AssertTrue(EM_OK == em_tmo_ack(context->Tmo, event));
                UnlockTimerTableEntry(timerId);
                LogPrint(ELogSeverityLevel_Error, \
                    "Failed to create a copy of the timeout message 0x%x (receiver: 0x%x, timer ID: 0x%x)", \
                    msgId, receiver, timerId);
            }

        } else {

            /* One-shot timer */

            /* Send the message directly */
            SendMessage(context->Message, context->Receiver);

            /* Free the timeout event */
            em_free(event);

            /* Timer expired - application is responsible for destroying it */
            context->State = ETimerState_Expired;

            UnlockTimerTableEntry(timerId);
        }
        break;

    case ETimerState_Cancelled:
        /* Timer was cancelled after the timeout event has been sent */

        /* Destroy the message and release the context */
        DestroyMessage(context->Message);

        /* Delete the timeout object */
        AssertTrue(EM_OK == em_tmo_delete(context->Tmo, &currentEvent));

        if (context->Period > 0) {

            /* Periodic timer expired - free the timeout event */
            AssertTrue(currentEvent != EM_EVENT_UNDEF);
            em_free(currentEvent);

        } else {

            /* One-shot timer expired - no event should be pending */
            AssertTrue(currentEvent == EM_EVENT_UNDEF);
        }

        /* Release the timer context */
        ReleaseTimerContext(timerId);
        UnlockTimerTableEntry(timerId);

        /* Free the timeout event */
        em_free(event);
        break;

    default:
        /* Timer in invalid state */

        /* This could occur if timer destruction commenced and before the timeout was cancelled, it fired.
         * The 'DestroyTimer' function would still see the state as ETimerState_Armed and so would destroy
         * the timer, but the event would still make its way here. */
        LogPrint(ELogSeverityLevel_Warning, "Received timeout event for timer 0x%x in invalid state: %d", \
            timerId, state);

        /* Free the timeout event */
        em_free(event);
        break;
    }

    UnlockTimerTableEntry(timerId);
}
