#include <timing/setup.h>
#include <timing/timer_instance.h>
#include <timing/timing_daemon.h>
#include <timing/timer_table.h>
#include <timing/timing.h>
#include <menabrea/timing.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

void TimingInit(void) {

    /* Start the daemon EO that will service timeout events */
    DeployTimingDaemon();

    /* Create the EM timer instance */
    CreateTimerInstance();

    /* Initialize the timer table for application's use */
    TimerTableInit();
}

void TimingTeardown(void) {

    TimerTableTeardown();
    DeleteTimerInstance();
    TimingDaemonTeardown();
}

void RetireAllTimers(void) {

    /* Timers run asynchronously with regards to platform code (completely within EM),
     * so we add a terminal state to their state machines. The timing daemon recognizes
     * this "retired" state and shall drop any outstanding notifications from expired timers. */
    LogPrint(ELogSeverityLevel_Info, "Retiring all timers...");

    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        RetireTimer(i);
    }
}
