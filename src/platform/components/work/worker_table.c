
#include "worker_table.h"
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

static SWorkerContext * s_workerTable[MAX_WORKER_COUNT];
static TWorkerId s_globalWorkerId = WORKER_ID_INVALID;

void WorkerTableInit(TWorkerId globalWorkerId) {

    s_globalWorkerId = globalWorkerId;
    LogPrint(ELogSeverityLevel_Info, "Global worker ID set to 0x%x", \
        s_globalWorkerId);

    int cores = em_core_count();
    /* Take into account per-core application private data - align with cache line */
    size_t entrySize = ENV_CACHE_LINE_SIZE_ROUNDUP(sizeof(SWorkerContext) + cores * sizeof(void *));
    size_t tableSize = entrySize * MAX_WORKER_COUNT;
    LogPrint(ELogSeverityLevel_Info, \
        "Creating worker table in shared memory - max workers: %d, entry size: %ld, table size: %ld...", \
        MAX_WORKER_COUNT, entrySize, tableSize);

    /* Do one big allocation and then set up the pointers to reference parts of it */
    void * tableBase = env_shared_malloc(tableSize);
    AssertTrue(tableBase != NULL);

    /* Set up the pointers and initialize the entries */
    for (TWorkerId i = 0; i < MAX_WORKER_COUNT; i++) {

        s_workerTable[i] = (SWorkerContext *)((u8*) tableBase + i * entrySize);
        env_spinlock_init(&s_workerTable[i]->Lock);
        ResetContext(s_workerTable[i]);
    }
}

void WorkerTableTeardown(void) {

    void * tableBase = s_workerTable[0];
    for (TWorkerId i = 0; i < MAX_WORKER_COUNT; i++) {

        /* Unset all the pointers */
        s_workerTable[i] = NULL;
    }
    /* Free the shared memory */
    env_shared_free(tableBase);
}

SWorkerContext * ReserveWorkerContext(TWorkerId workerId) {

    if (workerId == WORKER_ID_INVALID) {

        /* Dynamic ID allocation */

        for (TWorkerId id = WORKER_ID_DYNAMIC_BASE; id < MAX_WORKER_COUNT; id++) {

            /* Find free slot */
            if (s_workerTable[id]->State == EWorkerState_Inactive) {

                /* Lock the entry and retest the state */
                LockWorkerTableEntry(id);
                if (s_workerTable[id]->State == EWorkerState_Inactive) {

                    /* Free slot found, reserve it */
                    s_workerTable[id]->State = EWorkerState_Deploying;
                    s_workerTable[id]->WorkerId = MakeWorkerIdGlobal(id);
                    UnlockWorkerTableEntry(id);
                    return s_workerTable[id];
                }

                /* Contention with another registration, continue looking */
                UnlockWorkerTableEntry(id);
            }
        }

        LogPrint(ELogSeverityLevel_Critical, "No free worker IDs found!");
        return NULL;

    } else {

        /* Static ID provided */

        TWorkerId localId = WorkerIdGetLocal(workerId);
        FAIL_RESERVE_IF(localId >= MAX_WORKER_COUNT, Warning, \
            "Requested invalid worker ID: 0x%x", workerId);

        FAIL_RESERVE_IF(localId >= WORKER_ID_DYNAMIC_BASE, Warning, \
            "Requested worker ID in the dynamic range: 0x%x", workerId);

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
        s_workerTable[localId]->WorkerId = MakeWorkerIdGlobal(localId);
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
    env_spinlock_lock(&s_workerTable[localId]->Lock);
}

void UnlockWorkerTableEntry(TWorkerId workerId) {

    /* Use the local part to index the table */
    TWorkerId localId = WorkerIdGetLocal(workerId);
    AssertTrue(localId < MAX_WORKER_COUNT);
    env_spinlock_unlock(&s_workerTable[localId]->Lock);
}

TWorkerId GetGlobalWorkerId(void) {

    /* Assert global ID configured for this node */
    AssertTrue(s_globalWorkerId != WORKER_ID_INVALID);
    return s_globalWorkerId;
}

TWorkerId MakeWorkerIdGlobal(TWorkerId localId) {

    /* Assert global ID configured for this node */
    AssertTrue(s_globalWorkerId != WORKER_ID_INVALID);
    /* Shift the global ID by the local part bits and mask it */
    TWorkerId globalPart = (s_globalWorkerId << WORKER_LOCAL_ID_BITS) & WORKER_GLOBAL_ID_MASK;
    /* Remove any global ID bits set by the caller */
    TWorkerId localPart = localId & WORKER_LOCAL_ID_MASK;
    /* OR the two parts together */
    return globalPart | localPart;
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
    for (int i = 0; i < em_core_count(); i++) {

        context->Private[i] = NULL;
    }

    /* Clear event buffer */
    for (int i = 0; i < MESSAGE_BUFFER_LENGTH; i++) {

        context->MessageBuffer[i] = MESSAGE_INVALID;
    }
}
