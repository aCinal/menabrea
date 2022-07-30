
#ifndef PLATFORM_COMPONENTS_TIMING_TIMER_TABLE_H
#define PLATFORM_COMPONENTS_TIMING_TIMER_TABLE_H

#include <menabrea/timing.h>
#include <menabrea/common.h>
#include <menabrea/workers.h>
#include <event_machine/add-ons/event_machine_timer.h>

typedef enum ETimerState {
    ETimerState_Invalid = 0,
    ETimerState_Idle,
    ETimerState_Armed,
    ETimerState_Destroyed
} ETimerState;

typedef struct STimerContext {
    em_tmo_t Tmo;
    env_spinlock_t Lock;
    TMessage Message;
    TWorkerId Receiver;
    em_timer_tick_t Period;
    em_timer_tick_t PreviousExpiration;
    u32 SkipEvents;
    TTimerId TimerId;
    ETimerState State;
} STimerContext;

void TimerTableInit(void);
void TimerTableTeardown(void);
STimerContext * ReserveTimerContext(void);
void ReleaseTimerContext(TTimerId timerId);
STimerContext * FetchTimerContext(TTimerId timerId);
void LockTimerTableEntry(TTimerId timerId);
void UnlockTimerTableEntry(TTimerId timerId);

#endif /* PLATFORM_COMPONENTS_TIMING_TIMER_TABLE_H */
