#include <timing/timer_table.h>
#include <timing/timing_daemon.h>
#include <timing/timer_instance.h>
#include <menabrea/timing.h>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <event_machine/add-ons/event_machine_timer.h>
#include <event_machine.h>

static inline void ChangeStateFromArmedToIdle(STimerContext * context);
static inline void FinalizeTimerDestruction(STimerContext * context);

TTimerId CreateTimer(const char * name) {

    if (unlikely(name == NULL)) {

        RaiseException(EExceptionFatality_NonFatal, "Passed NULL pointer for timer name");
        return TIMER_ID_INVALID;
    }

    /* Reserve the context */
    STimerContext * context = ReserveTimerContext();
    if (unlikely(context == NULL)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to reserve context for timer '%s'", \
            __FUNCTION__, name);
        return TIMER_ID_INVALID;
    }

    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(context->Name, name, sizeof(context->Name) - 1);
    context->Name[sizeof(context->Name) - 1] = '\0';

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
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to create a timeout object for timer '%s'", \
            __FUNCTION__, name);
        return TIMER_ID_INVALID;
    }

    return context->TimerId;
}

TTimerId ArmTimer(TTimerId timerId, u64 expires, u64 period, TMessage message, TWorkerId receiver) {

    if (unlikely(timerId >= MAX_TIMER_COUNT)) {

        RaiseException(EExceptionFatality_NonFatal, "Timer ID 0x%x out of range", \
            timerId);
        return TIMER_ID_INVALID;
    }

    if (unlikely(MESSAGE_INVALID == message)) {

        RaiseException(EExceptionFatality_NonFatal, "Tried arming timer 0x%x with invalid message", \
            timerId);
        return TIMER_ID_INVALID;
    }

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    /* Check timer state */
    ETimerState state = context->State;
    if (state != ETimerState_Idle) {

        /* Note that this block correctly handles the case when the timer has already been retired. It is still exceptional behaviour
         * on the part of the application to try and arm a timer in exit code, so raising the exception is reasonable. */

        UnlockTimerTableEntry(timerId);
        RaiseException(EExceptionFatality_NonFatal, "Timer 0x%x in invalid state: %d (message ID: 0x%x, receiver: 0x%x)", \
            timerId, state, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    /* We have a valid timer, only now does it make sense to make introductions */
    char timerName[MAX_TIMER_NAME_LEN];

    /* Create a timeout event - to be received by the timing daemon */
    em_event_t timeoutEvent = em_alloc(sizeof(TTimerId), EM_EVENT_TYPE_SW, EM_POOL_DEFAULT);
    if (unlikely(timeoutEvent == EM_EVENT_UNDEF)) {

        /* Copy the name before releasing the lock */
        (void) strcpy(timerName, context->Name);
        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to allocate timeout event for timer '%s' (timer ID: 0x%x, message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, timerName, timerId, GetMessageId(message), receiver);
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

        /* Copy the name before releasing the lock */
        (void) strcpy(timerName, context->Name);
        UnlockTimerTableEntry(timerId);
        em_free(timeoutEvent);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to arm timer '%s' (timer ID: 0x%x, message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, timerName, timerId, GetMessageId(message), receiver);
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

void RetireTimer(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    char timerName[MAX_TIMER_NAME_LEN];
    em_tmo_t tmo = context->Tmo;
    ETimerState state = context->State;
    em_event_t currentEvent = EM_EVENT_UNDEF;
    em_status_t status;

    switch (state) {
    case ETimerState_Invalid:
        /* Timer not in use - just transition to state retired */

        context->State = ETimerState_Retired;
        UnlockTimerTableEntry(timerId);

        break;

    case ETimerState_Idle:
        /* Timer allocated by the application, but not running - release its context and
         * immediately mark it retired */

        /* Release the EM timeout object and context */
        FinalizeTimerDestruction(context);
        context->State = ETimerState_Retired;
        UnlockTimerTableEntry(timerId);
        break;

    case ETimerState_Armed:
        /* Disarm the timer, but transition straight to state retired, and ignore setting
         * the SkipEvents counter if event is already in flight */
        status = em_tmo_cancel(tmo, &currentEvent);
        switch (status) {
        case EM_OK:
            /* Timeout cancelled cleanly - timeout event will not be sent */

            /* Event must be valid - free it */
            AssertTrue(currentEvent != EM_EVENT_UNDEF);
            em_free(currentEvent);

            /* Transition to idle state and release the message associated with the timer */
            ChangeStateFromArmedToIdle(context);
            /* Release the EM timeout object and context */
            FinalizeTimerDestruction(context);
            /* Transition to the terminal state */
            context->State = ETimerState_Retired;
            UnlockTimerTableEntry(timerId);
            break;

        case EM_ERR_BAD_STATE:
            /* Timer in bad state - event has already been sent - but at this point (in shutdown mode)
             * it is no use to notify the timing daemon about events to be skipped - just transition
             * to state retired and let the daemon work off of that. */

            /* One-shot EM timers are in use - no event should have been returned */
            AssertTrue(currentEvent == EM_EVENT_UNDEF);

            /* Transition to idle state and release the message associated with the timer */
            ChangeStateFromArmedToIdle(context);
            /* Release the EM timeout object and context */
            FinalizeTimerDestruction(context);
            /* Transition to the terminal state */
            context->State = ETimerState_Retired;
            /* Copy the name before releasing the lock for debug purposes */
            (void) strcpy(timerName, context->Name);
            UnlockTimerTableEntry(timerId);

            LogPrint(ELogSeverityLevel_Debug, "%s(): Retired timer '%s' (0x%x), but an event has been sent already", \
                __FUNCTION__, timerName, timerId);
            break;

        default:
            /* Timer framework error */

            /* Copy the name before releasing the lock */
            (void) strcpy(timerName, context->Name);
            UnlockTimerTableEntry(timerId);
            RaiseException(EExceptionFatality_Fatal, \
                "em_tmo_cancel() returned %d for timer '%s' (0x%x)", \
                status, timerName, timerId);
            break;
        }
        break;

    case ETimerState_Destroyed:
        /* Some timeout events are still in flight. This is ok, the timing daemon should handle them
         * correctly if the timer is in state retired. */

        /* We can safely force-destroy the timer here */
        FinalizeTimerDestruction(context);
        context->State = ETimerState_Retired;
        UnlockTimerTableEntry(timerId);

        break;

    default:
        /* Timer in corrupted state */

        UnlockTimerTableEntry(timerId);
        RaiseException(EExceptionFatality_Fatal, \
            "Timer 0x%x in corrupted state: %d", timerId, state);
        break;
    }
}

TTimerId DisarmTimer(TTimerId timerId) {

    if (unlikely(timerId >= MAX_TIMER_COUNT)) {

        RaiseException(EExceptionFatality_NonFatal, "Timer ID 0x%x out of range", \
            timerId);
        return TIMER_ID_INVALID;
    }

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    char timerName[MAX_TIMER_NAME_LEN];
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
            /* Timeout cancelled cleanly - timeout event will not be sent */

            /* Event must be valid - free it */
            AssertTrue(currentEvent != EM_EVENT_UNDEF);
            em_free(currentEvent);

            /* Transition to idle state */
            ChangeStateFromArmedToIdle(context);
            UnlockTimerTableEntry(timerId);
            break;

        case EM_ERR_BAD_STATE:
            /* Timer in bad state - event has already been sent. Tell the timing daemon to ignore it
             * and destroy the timeout object */

            /* Use a counter and not a flag in case the timing daemon is starved and the application
             * manages to arm and disarm the timer multiple times */
            context->SkipEvents++;

            /* One-shot EM timers are in use - no event should have been returned */
            AssertTrue(currentEvent == EM_EVENT_UNDEF);

            /* Transition to idle state */
            ChangeStateFromArmedToIdle(context);

            /* Copy the name before releasing the lock */
            (void) strcpy(timerName, context->Name);
            UnlockTimerTableEntry(timerId);

            LogPrint(ELogSeverityLevel_Debug, \
                "%s(): Disarmed timer '%s' (0x%x), but an event has been sent already", \
                __FUNCTION__, timerName, timerId);
            break;

        default:
            /* Timer framework error */

            /* Copy the name before releasing the lock */
            (void) strcpy(timerName, context->Name);
            UnlockTimerTableEntry(timerId);
            RaiseException(EExceptionFatality_Fatal, \
                "em_tmo_cancel() returned %d for timer '%s' (0x%x)", \
                status, timerName, timerId);
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
        AssertTrue(context->PreviousExpiration == 0);

        UnlockTimerTableEntry(timerId);
        return timerId;

    case ETimerState_Retired:
        UnlockTimerTableEntry(timerId);
        /* Allow calling DisarmTimer for retired timers, but issue a diagnostic nonetheless */
        LogPrint(ELogSeverityLevel_Info, "%s(): Timer 0x%x already retired", \
            __FUNCTION__, timerId);
        /* Return ID to indicate success */
        return timerId;

    default:
        /* Timer in invalid state */

        UnlockTimerTableEntry(timerId);
        RaiseException(EExceptionFatality_NonFatal, "Timer 0x%x in invalid state: %d", \
            timerId, state);
        return TIMER_ID_INVALID;
    }
}

void DestroyTimer(TTimerId timerId) {

    if (unlikely(timerId >= MAX_TIMER_COUNT)) {

        RaiseException(EExceptionFatality_NonFatal, "Timer ID 0x%x out of range", \
            timerId);
        return;
    }

    LockTimerTableEntry(timerId);
    STimerContext * context = FetchTimerContext(timerId);

    ETimerState state = context->State;
    if (unlikely(state == ETimerState_Retired)) {

        UnlockTimerTableEntry(timerId);
        /* Allow calling DestroyTimer for retired timers, but issue a diagnostic nonetheless */
        LogPrint(ELogSeverityLevel_Info, "%s(): Timer 0x%x already retired", \
            __FUNCTION__, timerId);
        return;
    }

    if (unlikely(state != ETimerState_Idle)) {

        UnlockTimerTableEntry(timerId);
        RaiseException(EExceptionFatality_NonFatal, "Timer 0x%x in invalid state: %d", \
            timerId, state);
        return;
    }

    /* We have a valid timer, only now does it make sense to make introductions */
    char timerName[MAX_TIMER_NAME_LEN];

    if (context->SkipEvents > 0) {

        /* Timer events already pushed to the daemon queue but not yet handled - do not release the context
         * yet but let the timing daemon do it instead */
        context->State = ETimerState_Destroyed;
        /* Copy the name before releasing the lock */
        (void) strcpy(timerName, context->Name);
        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Debug, \
            "%s(): Destruction of timer '%s' (0x%x) delayed until the timing daemon receives all timeout events", \
            __FUNCTION__, timerName, timerId);

    } else {

        /* Copy the name before destroying the timer */
        (void) strcpy(timerName, context->Name);
        /* All timeout events handled - timer can be safely destroyed */
        FinalizeTimerDestruction(context);
        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Debug, "%s(): Cleanly destroyed timer '%s' (0x%x)", \
            __FUNCTION__, timerName, timerId);
    }
}

static inline void ChangeStateFromArmedToIdle(STimerContext * context) {

    /* Destroy the message */
    DestroyMessage(context->Message);
    context->Message = MESSAGE_INVALID;
    context->Receiver = WORKER_ID_INVALID;

    /* Reset periodic timer state */
    context->Period = 0;
    context->PreviousExpiration = 0;

    /* Do the actual state transition */
    context->State = ETimerState_Idle;
}

static inline void FinalizeTimerDestruction(STimerContext * context) {

    em_event_t currentEvent = EM_EVENT_UNDEF;
    /* Delete the timeout object */
    AssertTrue(EM_OK == em_tmo_delete(context->Tmo, &currentEvent));
    context->Tmo = EM_TMO_UNDEF;
    /* No event could have been returned if the timer had been properly
     * cancelled */
    AssertTrue(currentEvent == EM_EVENT_UNDEF);

    /* Sanity-check that no message leak occurs */
    AssertTrue(context->Message == MESSAGE_INVALID);
    AssertTrue(context->Receiver == WORKER_ID_INVALID);
    /* Release the context */
    ReleaseTimerContext(context->TimerId);
}
