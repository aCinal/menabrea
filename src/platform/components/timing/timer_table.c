#include <timing/timer_table.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/common.h>

static void ResetContext(STimerContext * context);

static inline TTimerId AllocateTimerId(void);
static inline void ReleaseTimerId(TTimerId timerId);

typedef struct STimerIdFifo {
    TSpinlock Lock;
    u32 GetIndex;
    u32 PutIndex;
    u32 FreeIds;
    TTimerId IdPool[MAX_TIMER_COUNT];
} STimerIdFifo;

static STimerIdFifo * s_idFifo;
static STimerContext * s_timerTable[MAX_TIMER_COUNT];

void TimerTableInit(void) {

    /* Align with cache line */
    size_t entrySize = ENV_CACHE_LINE_SIZE_ROUNDUP(sizeof(STimerContext));
    size_t tableSize = entrySize * MAX_TIMER_COUNT;
    size_t idFifoSize = ENV_CACHE_LINE_SIZE_ROUNDUP(sizeof(STimerIdFifo));
    size_t totalAllocationSize = idFifoSize + tableSize;
    LogPrint(ELogSeverityLevel_Info, \
        "Creating timer table in shared memory - max timers: %d, entry size: %ld, table size: %ld, ID fifo size: %ld, total: %ld", \
        MAX_TIMER_COUNT, entrySize, tableSize, idFifoSize, totalAllocationSize);

    /* Do one big allocation and then set up the pointers to reference parts of it */
    void * startAddr = env_shared_malloc(totalAllocationSize);
    AssertTrue(startAddr != NULL);

    /* Initialize the timer ID FIFO */
    s_idFifo = (STimerIdFifo *) startAddr;
    s_idFifo->GetIndex = 0;
    s_idFifo->PutIndex = 0;
    s_idFifo->FreeIds = MAX_TIMER_COUNT;
    SpinlockInit(&s_idFifo->Lock);
    for (TWorkerId i = 0; i < MAX_TIMER_COUNT; i++) {

        s_idFifo->IdPool[i] = i;
    }

    void * tableBase = (u8 *) startAddr + idFifoSize;
    /* Set up the pointers and initialize the table entries */
    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        s_timerTable[i] = (STimerContext *)((u8*) tableBase + i * entrySize);
        /* Initialize the timer IDs once at startup */
        s_timerTable[i]->TimerId = i;
        SpinlockInit(&s_timerTable[i]->Lock);
        ResetContext(s_timerTable[i]);
    }
}

void TimerTableTeardown(void) {

    /* Recover the start address of the shared memory */
    void * startAddr = s_idFifo;
    for (TTimerId i = 0; i < MAX_TIMER_COUNT; i++) {

        /* Unset all the pointers */
        s_timerTable[i] = NULL;
    }
    /* Free the shared memory */
    env_shared_free(startAddr);
}

STimerContext * ReserveTimerContext(void) {

    TTimerId id = AllocateTimerId();
    if (unlikely(id == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Critical, "No free timer IDs found");
        return NULL;
    }

    LockTimerTableEntry(id);
    /* Assert consistency between the timer table and the timer ID FIFO */
    AssertTrue(s_timerTable[id]->State == ETimerState_Invalid);
    s_timerTable[id]->State = ETimerState_Idle;
    UnlockTimerTableEntry(id);
    return s_timerTable[id];
}

void ReleaseTimerContext(TTimerId timerId) {

    /* Caller must ensure synchronization */

    AssertTrue(timerId < MAX_TIMER_COUNT);
    ResetContext(s_timerTable[timerId]);
    ReleaseTimerId(timerId);
}

STimerContext * FetchTimerContext(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);
    /* Do a sanity check each time a timer context is fetched */
    AssertTrue(s_timerTable[timerId]->TimerId == timerId);
    return s_timerTable[timerId];
}

void LockTimerTableEntry(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);
    SpinlockAcquire(&s_timerTable[timerId]->Lock);
}

void UnlockTimerTableEntry(TTimerId timerId) {

    AssertTrue(timerId < MAX_TIMER_COUNT);
    SpinlockRelease(&s_timerTable[timerId]->Lock);
}

static void ResetContext(STimerContext * context) {

    (void) memset(context->Name, 0, sizeof(context->Name));
    context->Tmo = EM_TMO_UNDEF;
    context->Message = MESSAGE_INVALID;
    context->Receiver = WORKER_ID_INVALID;
    context->Period = 0;
    context->PreviousExpiration = 0;
    context->SkipEvents = 0;
    context->State = ETimerState_Invalid;
}

static inline TTimerId AllocateTimerId(void) {

    TTimerId id = TIMER_ID_INVALID;
    SpinlockAcquire(&s_idFifo->Lock);
    if (s_idFifo->FreeIds > 0) {

        AssertTrue(s_idFifo->IdPool[s_idFifo->GetIndex] != TIMER_ID_INVALID);
        id = s_idFifo->IdPool[s_idFifo->GetIndex];
        /* Explicitly poison the allocated entry to detect any errors */
        s_idFifo->IdPool[s_idFifo->GetIndex] = TIMER_ID_INVALID;
        /* Move the allocation index */
        s_idFifo->GetIndex = (s_idFifo->GetIndex + 1) % MAX_TIMER_COUNT;
        s_idFifo->FreeIds--;
    }
    SpinlockRelease(&s_idFifo->Lock);
    return id;
}

static inline void ReleaseTimerId(TTimerId timerId) {

    SpinlockAcquire(&s_idFifo->Lock);
    AssertTrue(s_idFifo->IdPool[s_idFifo->PutIndex] == TIMER_ID_INVALID);
    s_idFifo->IdPool[s_idFifo->PutIndex] = timerId;
    s_idFifo->PutIndex = (s_idFifo->PutIndex + 1) % MAX_TIMER_COUNT;
    s_idFifo->FreeIds++;
    SpinlockRelease(&s_idFifo->Lock);
}
