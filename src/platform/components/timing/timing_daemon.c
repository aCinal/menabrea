#include "timing_daemon.h"
#include "timer_table.h"
#include "timer_instance.h"
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
static inline void HandleCleanTimeout(em_event_t event, STimerContext * context);
static inline void RearmPeriodicTimer(em_event_t event, STimerContext * context);

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

    ETimerState state = context->State;
    switch (state) {
    case ETimerState_Armed:
        /* Timer armed, i.e. either fired normally and we can handle the timeout cleanly
         * or it has been cancelled and rearmed before we got to handle any timeout events
         * here. */

        if (context->SkipEvents > 0) {

            /* Timer has been cancelled and rearmed - a timeout event for the cancelled
             * timer (that should be ignored) has already been sent to our queue */
            context->SkipEvents--;
            em_free(event);
            UnlockTimerTableEntry(timerId);
            LogPrint(ELogSeverityLevel_Debug, "%s(): Timer 0x%x cancelled and rearmed. Ignoring old event...", \
                __FUNCTION__, timerId);

        } else {

            /* Normal conditions - timer is armed (note that HandleCleanTimeout unlocks the
             * table entry internally) */
            HandleCleanTimeout(event, context);
        }
        break;

    case ETimerState_Idle:
        /* Timer has been cancelled - if we are here, then the timeout event must
         * have been sent before the cancellation */

        AssertTrue(context->SkipEvents > 0);
        context->SkipEvents--;
        em_free(event);

        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Debug, "%s(): Timer 0x%x cancelled and now idle. Ignoring late event...", \
            __FUNCTION__, timerId);
        break;

    case ETimerState_Destroyed:
        /* Timer has been cancelled and destroyed before all timeout events have been
         * handled. */

        AssertTrue(context->SkipEvents > 0);
        context->SkipEvents--;
        em_free(event);

        if (context->SkipEvents > 0) {

            UnlockTimerTableEntry(timerId);
            LogPrint(ELogSeverityLevel_Debug, "%s(): Timer 0x%x cancelled and destroyed. Ignoring late event...", \
                __FUNCTION__, timerId);

        } else {

            /* Only delete the timeout object and release the context
             * when all late events have been delivered and handled */
            em_event_t currentEvent;
            /* Delete the timeout object */
            AssertTrue(EM_OK == em_tmo_delete(context->Tmo, &currentEvent));
            context->Tmo = EM_TMO_UNDEF;
            /* No event could have been returned if the timer had been properly
             * cancelled */
            AssertTrue(currentEvent == EM_EVENT_UNDEF);

            /* Sanity-check that no message leak occurs */
            AssertTrue(context->Message == MESSAGE_INVALID);
            AssertTrue(context->Receiver == WORKER_ID_INVALID);

            ReleaseTimerContext(timerId);
            UnlockTimerTableEntry(timerId);
            LogPrint(ELogSeverityLevel_Debug, "%s(): Handled deferred destruction of timer 0x%x", \
                __FUNCTION__, timerId);
        }
        break;

    default:
        /* Timer in invalid state */

        UnlockTimerTableEntry(timerId);
        RaiseException(EExceptionFatality_Fatal, state, \
            "Daemon handling a timeout event for timer 0x%x in invalid state: %d", \
            timerId, state);
        break;
    }
}

static inline void HandleCleanTimeout(em_event_t event, STimerContext * context) {

    /* Caller must have called LockTimerTableEntry first */

    TTimerId timerId = context->TimerId;

    if (context->Period > 0) {

        /* Periodic timer */

        /* Send a copy of the original message */
        TMessage messageCopy = CopyMessage(context->Message);
        if (likely(messageCopy != MESSAGE_INVALID)) {

            SendMessage(messageCopy, context->Receiver);

            /* Rearm the timer - reuse the timeout event */
            RearmPeriodicTimer(event, context);

            /* Remain in armed state */

            UnlockTimerTableEntry(timerId);

        } else {

            /* Save the message metadata on stack before releasing the context lock */
            TMessageId msgId = GetMessageId(context->Message);
            TWorkerId receiver = context->Receiver;

            /* Rearm the timer - reuse the timeout event */
            RearmPeriodicTimer(event, context);

            /* Remain in armed state */

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

        /* Transition to idle state */
        context->Message = MESSAGE_INVALID;
        context->Receiver = WORKER_ID_INVALID;
        context->Period = 0;
        context->State = ETimerState_Idle;
        UnlockTimerTableEntry(timerId);
    }
}

static inline void RearmPeriodicTimer(em_event_t event, STimerContext * context) {

    /* Make first expiration unlikely and suffer a pipeline flush then */
    if (unlikely(context->PreviousExpiration == 0)) {

        /* First expiration - use current tick */
        context->PreviousExpiration = CurrentTick();
    }

    /* Calculate the absolute tick value of the next expiration */
    context->PreviousExpiration += context->Period;

    /* Try blindly setting the timeout and handle EM-ODP error if overrun
     * happened */
    em_status_t status = em_tmo_set_abs(
        context->Tmo,
        context->PreviousExpiration,
        event
    );
    switch (status) {
    case EM_OK:
        /* Timer rearmed cleanly */
        break;

    case EM_ERR_TOONEAR:
        /* Timer overrun */

        /* Set relative timeout and suffer a drift */
        AssertTrue(EM_OK == em_tmo_set_rel(
            context->Tmo,
            context->Period,
            event
        ));

        /* Set the previous expiration tick to now so as to not
         * suffer a pipeline flush when handling the next timeout
         * (cf. unlikely branch at the top of the function) */
        context->PreviousExpiration = CurrentTick();
        break;

    default:
        RaiseException(EExceptionFatality_Fatal, status, \
            "Failed to rearm periodic timer 0x%x - em_tmo_set_abs() returned %" PRI_STAT, \
            context->TimerId, status);
        break;
    }
}
