#include "timing.h"
#include "timer_table.h"
#include "timing_daemon.h"
#include "timer_instance.h"
#include "../work/worker_table.h"
#include <menabrea/timing.h>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <event_machine/add-ons/event_machine_timer.h>
#include <event_machine.h>

static inline void DestroyAllTimers(void);

void TimingInit(void) {

    /* Start the daemon EO that will service timeout events */
    DeployTimingDaemon();

    /* Create the EM timer instance */
    CreateTimerInstance();

    /* Initialize the timer table for application's use */
    TimerTableInit();
}

void TimingTeardown(void) {

    DestroyAllTimers();

    TimerTableTeardown();
    DeleteTimerInstance();
    TimingDaemonTeardown();
}

TTimerId CreateTimer(void) {

    /* Reserve the context */
    STimerContext * context = ReserveTimerContext();
    if (unlikely(context == NULL)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to reserve timer context", __FUNCTION__);
        return TIMER_ID_INVALID;
    }

    /* Create EM timeout object */
    context->Tmo = em_tmo_create(
        GetTimerInstance(),
        /* Only use one-shot timeouts and build periodic timers on top of this
         * as EM periodic timer API is poorly documented and awkward to work with */
        EM_TMO_FLAG_ONESHOT,
        GetTimingDaemonQueue()
    );

    if (unlikely(context->Tmo == EM_TMO_UNDEF)) {

        ReleaseTimerContext(context->TimerId);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to create a timeout object", __FUNCTION__);
        return TIMER_ID_INVALID;
    }

    return context->TimerId;
}

TTimerId ArmTimer(TTimerId timerId, u64 expires, u64 period, TMessage message, TWorkerId receiver) {

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    /* Check timer state */
    ETimerState state = context->State;
    if (state != ETimerState_Idle) {

        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Warning, "%s(): Timer 0x%x in invalid state: %d (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, timerId, state, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    /* Create a timeout event - to be received by the timing daemon */
    em_event_t timeoutEvent = em_alloc(sizeof(TTimerId), EM_EVENT_TYPE_SW, EM_POOL_DEFAULT);
    if (unlikely(timeoutEvent == EM_EVENT_UNDEF)) {

        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to allocate timeout event for timer 0x%x (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, timerId, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    /* Set the timer ID as payload of the timeout event */
    TTimerId * eventPayload = (TTimerId *) em_event_pointer(timeoutEvent);
    *eventPayload = context->TimerId;

    AssertTrue(context->Tmo != EM_TMO_UNDEF);
    /* Arm the timer */
    em_status_t status = em_tmo_set_rel(
        context->Tmo,
        MicrosecondsToTicks(expires),
        timeoutEvent
    );

    if (unlikely(status != EM_OK)) {

        UnlockTimerTableEntry(timerId);
        em_free(timeoutEvent);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to arm %s timer 0x%x (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, period > 0 ? "periodic" : "one-shot", timerId, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    /* Fill in the context */
    context->Message = message;
    context->Receiver = receiver;
    /* Store the period in ticks already */
    context->Period = MicrosecondsToTicks(period);
    /* Do a state transition - note that we still hold the lock, so even if the EM-level timeout has fired
     * by now, the state will still be consistent once we release the lock */
    context->State = ETimerState_Armed;

    UnlockTimerTableEntry(timerId);

    return context->TimerId;
}

TTimerId DisarmTimer(TTimerId timerId) {

    LockTimerTableEntry(timerId);

    STimerContext * context = FetchTimerContext(timerId);
    em_tmo_t tmo = context->Tmo;
    ETimerState state = context->State;
    em_event_t currentEvent = EM_EVENT_UNDEF;
    em_status_t status;

    switch (state) {
    case ETimerState_Armed:
        /* Timeout event not yet received by the timing daemon (one-shot) or
         * periodic timer still running - try cancelling */

        status = em_tmo_cancel(tmo, &currentEvent);
        switch (status) {
        case EM_OK:
            /* Timeout cancelled cleanly - timout event will not be sent */

            /* Event must be valid - free it */
            AssertTrue(currentEvent != EM_EVENT_UNDEF);
            em_free(currentEvent);

            /* Destroy the message */
            DestroyMessage(context->Message);
            context->Message = MESSAGE_INVALID;
            context->Receiver = WORKER_ID_INVALID;

            context->Period = 0;

            /* Do a state transition */
            context->State = ETimerState_Idle;
            UnlockTimerTableEntry(timerId);

            LogPrint(ELogSeverityLevel_Debug, "%s(): Cleanly disarmed timer 0x%x", \
                __FUNCTION__, timerId);
            break;

        case EM_ERR_BAD_STATE:
            /* Timer in bad state - event has already been sent. Tell the timing daemon to ignore it
             * and destroy the timeout object */

            /* Use a counter and not a flag in case the timing daemon is starved and the application
             * manages to arm and disarm the timer multiple times */
            context->SkipEvents++;

            /* One-shot EM timers are in use - no event should have been returned */
            AssertTrue(currentEvent == EM_EVENT_UNDEF);

            /* Destroy the message */
            DestroyMessage(context->Message);
            context->Message = MESSAGE_INVALID;
            context->Receiver = WORKER_ID_INVALID;

            context->Period = 0;

            /* Do a state transition */
            context->State = ETimerState_Idle;
            UnlockTimerTableEntry(timerId);

            LogPrint(ELogSeverityLevel_Debug, "%s(): Disarmed timer 0x%x, but an event has been sent already", \
                __FUNCTION__, timerId);
            break;

        default:
            /* Timer framework error */

            UnlockTimerTableEntry(timerId);
            RaiseException(EExceptionFatality_Fatal, status, "%s(): em_tmo_cancel() failed for timer 0x%x", \
                __FUNCTION__, timerId);
            break;
        }
        return timerId;

    case ETimerState_Idle:
        /* One-shot timer already expired and context cleaned in the timing daemon (or timer not armed at all,
         * either way allow this call) */

        /* Use the opportunity to do some sanity checks */
        AssertTrue(context->Message == MESSAGE_INVALID);
        AssertTrue(context->Receiver = WORKER_ID_INVALID);
        AssertTrue(context->Period == 0);

        UnlockTimerTableEntry(timerId);
        return timerId;

    default:
        /* Timer in invalid state */

        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Warning, "%s(): Timer 0x%x in invalid state: %d", \
            __FUNCTION__, timerId, state);
        return TIMER_ID_INVALID;
    }
}

void DestroyTimer(TTimerId timerId) {

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    ETimerState state = context->State;
    if (state != ETimerState_Idle) {

        LogPrint(ELogSeverityLevel_Warning, "%s(): Timer 0x%x in invalid state: %d", \
            __FUNCTION__, timerId, state);
        UnlockTimerTableEntry(timerId);
        return;
    }

    if (context->SkipEvents > 0) {

        /* Timer events already pushed to the daemon queue but not yet handled - do not release the context
         * yet but let the timing daemon do it instead */
        context->State = ETimerState_Destroyed;
        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Debug, \
            "%s(): Destruction of timer 0x%x delayed until the timing daemon receives all timeout events", \
            __FUNCTION__, timerId);

    } else {

        /* All timeout events handled - timer can be safely destroyed */

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
        LogPrint(ELogSeverityLevel_Debug, "%s(): Cleanly destroyed timer 0x%x", \
            __FUNCTION__, timerId);
    }
}

void CancelAllTimers(void) {

    LogPrint(ELogSeverityLevel_Debug, "%s(): Cancelling any remaining timers...", \
        __FUNCTION__);

    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        /* Do not do any locking (simulate application perspective) - a timer may
         * change state from ETimerState_Armed to ETimerState_Idle in the meantime
         * - the implementation should handle this gracefully. */
        STimerContext * context = FetchTimerContext(i);
        if (context->State == ETimerState_Armed) {

            DisarmTimer(i);
        }
    }
}

static inline void DestroyAllTimers(void) {

    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        STimerContext * context = FetchTimerContext(i);
        /* Only idle and inactive timers can exist at this point */
        AssertTrue(context->State != ETimerState_Armed);
        AssertTrue(context->State != ETimerState_Destroyed);
        if (context->State == ETimerState_Idle) {

            DestroyTimer(i);
        }
    }
}
