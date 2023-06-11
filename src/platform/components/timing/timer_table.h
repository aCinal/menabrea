
#ifndef PLATFORM_COMPONENTS_TIMING_TIMER_TABLE_H
#define PLATFORM_COMPONENTS_TIMING_TIMER_TABLE_H

#include <menabrea/timing.h>
#include <menabrea/common.h>
#include <menabrea/workers.h>
#include <event_machine/add-ons/event_machine_timer.h>

#define MAX_TIMER_NAME_LEN  16

typedef enum ETimerState {
    ETimerState_Invalid = 0,
    ETimerState_Idle,
    ETimerState_Armed,
    ETimerState_Destroyed
} ETimerState;

typedef struct STimerContext {
    em_tmo_t Tmo;
    TSpinlock Lock;
    em_timer_tick_t Period;
    em_timer_tick_t PreviousExpiration;
    u32 SkipEvents;
    TMessage Message;
    TWorkerId Receiver;
    TTimerId TimerId;
    ETimerState State;
    char Name[MAX_TIMER_NAME_LEN];
    /* Pad to have the size be a multiple of the cache line size */
    void * _pad[0] ENV_CACHE_LINE_ALIGNED;
} STimerContext;

void TimerTableInit(void);
void TimerTableTeardown(void);
STimerContext * ReserveTimerContext(void);
void ReleaseTimerContext(TTimerId timerId);
STimerContext * FetchTimerContext(TTimerId timerId);
void LockTimerTableEntry(TTimerId timerId);
void UnlockTimerTableEntry(TTimerId timerId);

#endif /* PLATFORM_COMPONENTS_TIMING_TIMER_TABLE_H */
