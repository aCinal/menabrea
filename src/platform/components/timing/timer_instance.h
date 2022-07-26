
#ifndef PLATFORM_COMPONENTS_TIMING_TIMER_INSTANCE_H
#define PLATFORM_COMPONENTS_TIMING_TIMER_INSTANCE_H

#include <event_machine.h>
#include <event_machine/add-ons/event_machine_timer.h>
#include <menabrea/common.h>

void CreateTimerInstance(void);
void DeleteTimerInstance(void);
em_timer_t GetTimerInstance(void);
em_timer_tick_t MicrosecondsToTicks(u64 us);

#endif /* PLATFORM_COMPONENTS_TIMING_TIMER_INSTANCE_H */
