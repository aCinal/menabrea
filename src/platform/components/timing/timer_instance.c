#include "timer_instance.h"
#include <menabrea/exception.h>
#include <string.h>

static em_timer_t s_timerInstance = EM_TIMER_UNDEF;

void CreateTimerInstance(void) {

    em_timer_attr_t timerAttributes;
    em_timer_attr_init(&timerAttributes);
    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(timerAttributes.name, "platform_timer", sizeof(timerAttributes.name) - 1);
    timerAttributes.name[sizeof(timerAttributes.name) - 1] = '\0';
    /* Create the timer instance */
    s_timerInstance = em_timer_create(&timerAttributes);
    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);
}

void DeleteTimerInstance(void) {

    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);
    AssertTrue(EM_OK == em_timer_delete(s_timerInstance));
}

em_timer_t GetTimerInstance(void) {

    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);
    return s_timerInstance;
}

em_timer_tick_t MicrosecondsToTicks(u64 us) {

    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);
    return em_timer_ns_to_tick(s_timerInstance, us * 1000);
}
