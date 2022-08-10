#include <timing/setup.h>
#include <timing/timer_instance.h>
#include <timing/timing_daemon.h>
#include <timing/timer_table.h>
#include <menabrea/timing.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

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

void CancelAllTimers(void) {

    LogPrint(ELogSeverityLevel_Debug, "Cancelling any remaining timers...");

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
