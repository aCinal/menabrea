
#include <workers/worker_table.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/common.h>
#include <string.h>

#define FAIL_RESERVE_IF(cond, severity, format, ...) \
    do { \
        if (unlikely(cond)) { \
            LogPrint(ELogSeverityLevel_##severity, format, ##__VA_ARGS__); \
            return NULL; \
        } \
    } while (0)

static void ResetContext(SWorkerContext * context);
static inline TWorkerId AllocateDynamicLocalId(void);
static inline void ReleaseDynamicLocalId(TWorkerId localId);

typedef struct SDynamicIdFifo {
    TSpinlock Lock;
    u32 GetIndex;
    u32 PutIndex;
    u32 FreeIds;
    TWorkerId IdPool[DYNAMIC_WORKER_IDS_COUNT];
} SDynamicIdFifo;

static SDynamicIdFifo * s_idFifo;
static SWorkerContext * s_workerTable[MAX_WORKER_COUNT];
static TWorkerId s_ownNodeId = WORKER_ID_INVALID;

void WorkerTableInit(TWorkerId nodeId) {

    s_ownNodeId = nodeId;
    LogPrint(ELogSeverityLevel_Info, "Node ID set to 0x%x", s_ownNodeId);

    int cores = em_core_count();
    /* Take into account per-core application private data - align with cache line */
    size_t entrySize = ENV_CACHE_LINE_SIZE_ROUNDUP(sizeof(SWorkerContext) + cores * sizeof(void *));
    size_t tableSize = entrySize * MAX_WORKER_COUNT;
    size_t idFifoSize = ENV_CACHE_LINE_SIZE_ROUNDUP(sizeof(SDynamicIdFifo));
    size_t totalAllocationSize = idFifoSize + tableSize;
    LogPrint(ELogSeverityLevel_Info, \
        "Creating worker table in shared memory - max workers: %d, entry size: %ld, table size: %ld, ID fifo size: %ld, total: %ld...", \
        MAX_WORKER_COUNT, entrySize, tableSize, idFifoSize, totalAllocationSize);

    /* Do one big allocation and then set up the pointers to reference parts of it */
    void * startAddr = env_shared_malloc(totalAllocationSize);
    AssertTrue(startAddr != NULL);

    /* Initialize the dynamic ID FIFO */
    s_idFifo = (SDynamicIdFifo *) startAddr;
    s_idFifo->GetIndex = 0;
    s_idFifo->PutIndex = 0;
    s_idFifo->FreeIds = DYNAMIC_WORKER_IDS_COUNT;
    SpinlockInit(&s_idFifo->Lock);
    for (TWorkerId i = 0; i < DYNAMIC_WORKER_IDS_COUNT; i++) {

        s_idFifo->IdPool[i] = WORKER_ID_DYNAMIC_BASE + i;
    }

    void * tableBase = (u8 *) startAddr + idFifoSize;
    /* Set up the pointers and initialize the table entries */
    for (TWorkerId i = 0; i < MAX_WORKER_COUNT; i++) {

        s_workerTable[i] = (SWorkerContext *)((u8*) tableBase + i * entrySize);
        SpinlockInit(&s_workerTable[i]->Lock);
        ResetContext(s_workerTable[i]);
    }
}

void WorkerTableTeardown(void) {

    /* Recover the start address of the shared memory */
    void * startAddr = s_idFifo;
    for (TWorkerId i = 0; i < MAX_WORKER_COUNT; i++) {

        /* Unset all the pointers */
        s_workerTable[i] = NULL;
    }
    /* Free the shared memory */
    env_shared_free(startAddr);
}

SWorkerContext * ReserveWorkerContext(TWorkerId workerId) {

    if (workerId == WORKER_ID_INVALID) {

        /* Dynamic ID allocation */

        TWorkerId dynamicId = AllocateDynamicLocalId();

        if (unlikely(dynamicId == WORKER_ID_INVALID)) {

            LogPrint(ELogSeverityLevel_Critical, "No free worker IDs found!");
            return NULL;
        }

        LockWorkerTableEntry(dynamicId);
        /* Assert consistency between the worker table and the dynamic ID FIFO */
        AssertTrue(s_workerTable[dynamicId]->State == EWorkerState_Inactive);
        s_workerTable[dynamicId]->State = EWorkerState_Deploying;
        s_workerTable[dynamicId]->WorkerId = MakeWorkerId(GetOwnNodeId(), dynamicId);
        UnlockWorkerTableEntry(dynamicId);
        return s_workerTable[dynamicId];

    } else {

        /* Static ID provided */

        TWorkerId localId = WorkerIdGetLocal(workerId);
        FAIL_RESERVE_IF(localId >= MAX_WORKER_COUNT, Warning, \
            "Requested invalid worker ID: 0x%x", workerId);

        FAIL_RESERVE_IF(localId >= WORKER_ID_DYNAMIC_BASE, Warning, \
            "Requested worker ID 0x%x in the dynamic range: 0x%x-0x%x", \
            workerId, WORKER_ID_DYNAMIC_BASE, MAX_WORKER_COUNT - 1);

        LockWorkerTableEntry(workerId);
        if (s_workerTable[localId]->State != EWorkerState_Inactive) {

            EWorkerState state = s_workerTable[localId]->State;
            UnlockWorkerTableEntry(workerId);
            LogPrint(ELogSeverityLevel_Warning, "Attempted to register worker with static ID 0x%x twice. Current worker in state: %d", \
                workerId, state);
            return NULL;
        }
        /* Reserve the entry */
        s_workerTable[localId]->State = EWorkerState_Deploying;
        s_workerTable[localId]->WorkerId = MakeWorkerId(GetOwnNodeId(), localId);
        UnlockWorkerTableEntry(workerId);
        return s_workerTable[localId];
    }
}

void ReleaseWorkerContext(TWorkerId workerId) {

    LockWorkerTableEntry(workerId);
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    /* Assert the state is either EWorkerState_Terminating or
     * EWorkerState_Deploying - releasing active workers is
     * not allowed to prevent any race conditions */
    AssertTrue(s_workerTable[localId]->State != EWorkerState_Active);
    ResetContext(s_workerTable[localId]);
    if (localId >= WORKER_ID_DYNAMIC_BASE) {
        /* Recycle dynamic local ID */
        ReleaseDynamicLocalId(localId);
    }
    UnlockWorkerTableEntry(workerId);
}

SWorkerContext * FetchWorkerContext(TWorkerId workerId) {

    /* Use the local part to index the table */
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    return s_workerTable[localId];
}

void MarkDeploymentSuccessful(TWorkerId workerId) {

    /* Caller must ensure synchronization */

    /* Use the local part to index the table */
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    /* Assert deployment in progress */
    AssertTrue(s_workerTable[localId]->State == EWorkerState_Deploying);
    s_workerTable[localId]->State = EWorkerState_Active;
}

void MarkTeardownInProgress(TWorkerId workerId) {

    /* Caller must ensure synchronization */

    /* Use the local part to index the table */
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    /* Assert worker active */
    AssertTrue(s_workerTable[localId]->State == EWorkerState_Active);
    s_workerTable[localId]->State = EWorkerState_Terminating;
}

void LockWorkerTableEntry(TWorkerId workerId) {

    /* Use the local part to index the table */
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    SpinlockAcquire(&s_workerTable[localId]->Lock);
}

void UnlockWorkerTableEntry(TWorkerId workerId) {

    /* Use the local part to index the table */
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    SpinlockRelease(&s_workerTable[localId]->Lock);
}

TWorkerId GetOwnNodeId(void) {

    /* Assert node ID has been configured */
    AssertTrue(s_ownNodeId != WORKER_ID_INVALID);
    return s_ownNodeId;
}

static void ResetContext(SWorkerContext * context) {

    /* Reset the state */
    context->State = EWorkerState_Inactive;
    /* Clear user callbacks */
    context->UserInit = NULL;
    context->UserLocalInit = NULL;
    context->UserLocalExit = NULL;
    context->UserExit = NULL;
    context->WorkerBody = NULL;

    /* Clear internal data */
    (void) memset(context->Name, 0, sizeof(context->Name));
    context->CoreMask = 0;
    context->Parallel = false;
    context->Queue = EM_QUEUE_UNDEF;
    context->Eo = EM_EO_UNDEF;
    context->WorkerId = WORKER_ID_INVALID;
    context->TerminationRequested = false;

    /* Clear application private data */
    context->SharedData = NULL;
    for (int i = 0; i < em_core_count(); i++) {

        context->LocalData[i] = NULL;
    }

    /* Clear event buffer */
    for (int i = 0; i < MESSAGE_BUFFER_LENGTH; i++) {

        context->MessageBuffer[i] = MESSAGE_INVALID;
    }
}

static inline TWorkerId AllocateDynamicLocalId(void) {

    TWorkerId id = WORKER_ID_INVALID;
    SpinlockAcquire(&s_idFifo->Lock);
    if (s_idFifo->FreeIds > 0) {

        AssertTrue(s_idFifo->IdPool[s_idFifo->GetIndex] != WORKER_ID_INVALID);
        id = s_idFifo->IdPool[s_idFifo->GetIndex];
        /* Explicitly poison the allocated entry to detect any errors */
        s_idFifo->IdPool[s_idFifo->GetIndex] = WORKER_ID_INVALID;
        /* Move the allocation index */
        s_idFifo->GetIndex = (s_idFifo->GetIndex + 1) % DYNAMIC_WORKER_IDS_COUNT;
        s_idFifo->FreeIds--;
    }
    SpinlockRelease(&s_idFifo->Lock);
    return id;
}

static inline void ReleaseDynamicLocalId(TWorkerId localId) {

    AssertTrue(0 == WorkerIdGetNode(localId));
    SpinlockAcquire(&s_idFifo->Lock);
    AssertTrue(s_idFifo->IdPool[s_idFifo->PutIndex] == WORKER_ID_INVALID);
    s_idFifo->IdPool[s_idFifo->PutIndex] = localId;
    s_idFifo->PutIndex = (s_idFifo->PutIndex + 1) % DYNAMIC_WORKER_IDS_COUNT;
    s_idFifo->FreeIds++;
    SpinlockRelease(&s_idFifo->Lock);
}
