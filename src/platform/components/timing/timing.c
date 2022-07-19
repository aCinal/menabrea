#include "timing.h"
#include "timer_table.h"
#include "timing_daemon.h"
#include "../work/worker_table.h"
#include <menabrea/timing.h>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <event_machine/add-ons/event_machine_timer.h>
#include <event_machine.h>

static inline em_timer_tick_t MicrosecondsToTicks(u64 us);

static em_timer_t s_timerInstance = EM_TIMER_UNDEF;

void TimingInit(void) {

    /* Start the daemon EO that will service timeout events */
    DeployTimingDaemon();

    em_timer_attr_t timerAttributes;
    em_timer_attr_init(&timerAttributes);
    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(timerAttributes.name, "platform_timer", sizeof(timerAttributes.name) - 1);
    timerAttributes.name[sizeof(timerAttributes.name) - 1] = '\0';
    /* Create the timer instance */
    s_timerInstance = em_timer_create(&timerAttributes);
    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);

    /* Initialize the timer table for application's use */
    TimerTableInit();
}

void TimingTeardown(void) {

    /* TODO: Cancel any active timeouts */

    TimerTableTeardown();
    AssertTrue(EM_OK == em_timer_delete(s_timerInstance));
    TimingDaemonTeardown();
}

TTimerId CreateTimer(TMessage message, TWorkerId receiver, u64 expires, u64 period) {

    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);

    LogPrint(ELogSeverityLevel_Debug, "Creating %s timer with message 0x%x to 0x%x...", \
        period > 0 ? "periodic" : "one-shot", GetMessageId(message), receiver);

    /* Allocate a timer context */
    STimerContext * context = ReserveTimerContext();
    if (unlikely(context == NULL)) {

        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to reserve timer context (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    context->Message = message;
    context->Receiver = receiver;
    context->Period = period;

    /* Create EM timeout object */
    context->Tmo = em_tmo_create(
        s_timerInstance,
        period ? EM_TMO_FLAG_PERIODIC | EM_TMO_FLAG_NOSKIP : EM_TMO_FLAG_ONESHOT,
        GetTimingDaemonQueue()
    );

    if (unlikely(context->Tmo == EM_TMO_UNDEF)) {

        ReleaseTimerContext(context->TimerId);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to create timout object (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    em_event_t timeoutEvent = em_alloc(sizeof(TTimerId), EM_EVENT_TYPE_SW, EM_POOL_DEFAULT);
    if (unlikely(timeoutEvent == EM_EVENT_UNDEF)) {

        em_event_t currentEvent = EM_EVENT_UNDEF;
        (void) em_tmo_delete(context->Tmo, &currentEvent);
        /* No event has been associated with the timeout yet */
        AssertTrue(currentEvent == EM_EVENT_UNDEF);
        ReleaseTimerContext(context->TimerId);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to allocate timeout event (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    /* Set the timer ID as payload of the timeout event */
    TTimerId * eventPayload = (TTimerId *) em_event_pointer(timeoutEvent);
    *eventPayload = context->TimerId;

    /* Arm the timer */
    em_status_t status;
    context->State = ETimerState_Armed;
    if (period > 0) {

        /* Arm periodic timer - note that the em_tmo_set_periodic() API requires
         * absolute value for the start tick */
        em_timer_tick_t now = em_timer_current_tick(s_timerInstance);
        status = em_tmo_set_periodic(
            context->Tmo,
            now + MicrosecondsToTicks(expires),
            MicrosecondsToTicks(period),
            timeoutEvent
        );

    } else {

        /* Arm one-shot timer - use em_tmo_set_rel() API that uses relative value
         * of the timeout */
        status = em_tmo_set_rel(
            context->Tmo,
            MicrosecondsToTicks(expires),
            timeoutEvent
        );
    }

    if (unlikely(status != EM_OK)) {

        em_free(timeoutEvent);
        em_event_t currentEvent = EM_EVENT_UNDEF;
        (void) em_tmo_delete(context->Tmo, &currentEvent);
        /* No event has been associated with the timeout yet */
        AssertTrue(currentEvent == EM_EVENT_UNDEF);
        ReleaseTimerContext(context->TimerId);
        LogPrint(ELogSeverityLevel_Error, "%s(): Failed to arm the %s timer (message ID: 0x%x, receiver: 0x%x)", \
            __FUNCTION__, period > 0 ? "periodic" : "one-shot", GetMessageId(message), receiver);
        return TIMER_ID_INVALID;
    }

    return context->TimerId;
}

void DestroyTimer(TTimerId timerId) {

    LockTimerTableEntry(timerId);

    STimerContext * context = FetchTimerContext(timerId);
    em_tmo_t tmo = context->Tmo;
    ETimerState state = context->State;
    em_event_t currentEvent = EM_EVENT_UNDEF;

    switch (state) {
    case ETimerState_Armed:
        /* Timeout event not yet received by the timing daemon (one-shot) or
         * periodic timer still running - try cancelling */

        switch (em_tmo_cancel(tmo, &currentEvent)) {
        case EM_OK:
            /* Timeout cancelled cleanly - timout event will not be sent */

            /* Destroy the message */
            DestroyMessage(context->Message);

            /* Release the timer context - we do not need it anymore */
            ReleaseTimerContext(timerId);
            UnlockTimerTableEntry(timerId);

            /* Event must be valid - free it */
            AssertTrue(currentEvent != EM_EVENT_UNDEF);
            em_free(currentEvent);
            /* Timeout can be safely deleted */
            AssertTrue(EM_OK == em_tmo_delete(tmo, &currentEvent));
            /* This time no event can be returned (we just freed the timeout
             * event when cancelling) */
            AssertTrue(currentEvent == EM_EVENT_UNDEF);
            break;

        case EM_ERR_BAD_STATE:
            /* Timer in bad state - event has already been sent. Do not release the timer
             * context here - we'll do it in the timing daemon */

            context->State = ETimerState_Cancelled;
            UnlockTimerTableEntry(timerId);
            LogPrint(ELogSeverityLevel_Debug, "%s(): Timer 0x%x already expired. To be destroyed by the timing daemon", \
                __FUNCTION__, timerId);
            break;

        default:
            /* Timer framework error */

            UnlockTimerTableEntry(timerId);
            /* Don't bother printing the return value here - EM logs all errors
             * internally */
            LogPrint(ELogSeverityLevel_Error, "%s(): em_tmo_cancel() failed for timer 0x%x", \
                __FUNCTION__, timerId);
            break;
        }

        break;

    case ETimerState_Expired:
        /* One-shot timer already expired (handled by the timing daemon) - can be
         * safely cancelled */

        /* Destroy the message */
        DestroyMessage(context->Message);

        /* Release the timer context - we do not need it anymore */
        ReleaseTimerContext(timerId);
        UnlockTimerTableEntry(timerId);

        /* Delete the timeout object */
        AssertTrue(EM_OK == em_tmo_delete(tmo, &currentEvent));
        /* Timer expired - no event should be pending */
        AssertTrue(currentEvent == EM_EVENT_UNDEF);
        break;

    default:
        /* Timer in invalid state */

        UnlockTimerTableEntry(timerId);
        LogPrint(ELogSeverityLevel_Warning, "%s(): Timer 0x%x in invalid state: %d", \
            __FUNCTION__, timerId, state);
        break;
    }
}

static inline em_timer_tick_t MicrosecondsToTicks(u64 us) {

    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);
    return em_timer_ns_to_tick(s_timerInstance, us * 1000);
}
