#include "timer_table.h"
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/common.h>

static void ResetContext(STimerContext * context);

static STimerContext * s_timerTable[MAX_TIMER_COUNT];

void TimerTableInit(void) {

    /* Align with cache line */
    size_t entrySize = ENV_CACHE_LINE_SIZE_ROUNDUP(sizeof(STimerContext));
    size_t tableSize = entrySize * MAX_TIMER_COUNT;
    LogPrint(ELogSeverityLevel_Info, \
        "%s(): Creating timer table in shared memory - max timers: %d, entry size: %ld, table size: %ld", \
        __FUNCTION__, MAX_TIMER_COUNT, entrySize, tableSize);

    /* Do one big allocation and then set up the pointers to reference parts of it */
    void * tableBase = env_shared_malloc(tableSize);
    AssertTrue(tableBase != NULL);

    /* Set up the pointers and initialize the entries */
    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        s_timerTable[i] = (STimerContext *)((u8*) tableBase + i * sizeof(STimerContext));
        /* Initialize the timer IDs once at startup */
        s_timerTable[i]->TimerId = i;
        env_spinlock_init(&s_timerTable[i]->Lock);
        ResetContext(s_timerTable[i]);
    }
}

void TimerTableTeardown(void) {

    void * tableBase = s_timerTable[0];
    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        /* Unset all the pointers */
        s_timerTable[i] = NULL;
    }
    /* Free the shared memory */
    env_shared_free(tableBase);
}

STimerContext * ReserveTimerContext(void) {

    for (TTimerId id = 0; id < MAX_TIMER_COUNT; id++) {

        /* Find free slot */
        if (s_timerTable[id]->State == ETimerState_Invalid) {

            /* Lock the entry and retest the state */
            LockTimerTableEntry(id);
            if (s_timerTable[id]->State == ETimerState_Invalid) {

                /* Free slot found, reserve it */
                s_timerTable[id]->State = ETimerState_Idle;
                UnlockTimerTableEntry(id);
                return s_timerTable[id];
            }

            /* Contention with another registration, continue looking */
            UnlockTimerTableEntry(id);
        }
    }

    LogPrint(ELogSeverityLevel_Critical, "%s(): No free timer IDs found!", __FUNCTION__);
    return NULL;
}

void ReleaseTimerContext(TTimerId timerId) {

    /* Caller must ensure synchronization */

    AssertTrue(timerId < MAX_TIMER_COUNT);
    ResetContext(s_timerTable[timerId]);
}

STimerContext * FetchTimerContext(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);
    /* Do a sanity check each time a timer context is fetched */
    AssertTrue(s_timerTable[timerId]->TimerId == timerId);
    return s_timerTable[timerId];
}

void LockTimerTableEntry(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);
    env_spinlock_lock(&s_timerTable[timerId]->Lock);
}

void UnlockTimerTableEntry(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);
    env_spinlock_unlock(&s_timerTable[timerId]->Lock);
}

static void ResetContext(STimerContext * context) {

    context->Tmo = EM_TMO_UNDEF;
    context->Message = MESSAGE_INVALID;
    context->Receiver = WORKER_ID_INVALID;
    context->Period = 0;
    context->SkipEvents = 0;
    context->State = ETimerState_Invalid;
}
