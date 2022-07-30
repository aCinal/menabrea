#include "timer_instance.h"
#include <menabrea/exception.h>
#include <string.h>

static em_timer_t s_timerInstance = EM_TIMER_UNDEF;

void CreateTimerInstance(void) {

    em_timer_attr_t timerAttributes;
    em_timer_attr_init(&timerAttributes);

    /* Inquire about EM-ODP timer capability for the default clock source */
    em_timer_capability_t capa;
    AssertTrue(EM_OK == em_timer_capability(&capa, EM_TIMER_CLKSRC_DEFAULT));

    /* Set the timer attributes manually as em_timer_attr_init() uses a naive
     * strategy where it assumes a predefined resolution will be accepted by
     * ODP and only then inquires about timeout capability for this resolution
     * (and the linux-generic implementation of ODP also returns a constant,
     * namely MAX_TMO_NSEC, irrespective of the possibly invalid resolution). */
    timerAttributes.resparam.res_ns = capa.max_res.res_ns;
    timerAttributes.resparam.res_hz = 0;
    timerAttributes.resparam.max_tmo = capa.max_res.max_tmo;
    timerAttributes.resparam.min_tmo = capa.max_res.min_tmo;
    timerAttributes.resparam.clk_src = EM_TIMER_CLKSRC_DEFAULT;

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

em_timer_tick_t CurrentTick(void) {

    AssertTrue(s_timerInstance != EM_TIMER_UNDEF);
    return em_timer_current_tick(s_timerInstance);
}
