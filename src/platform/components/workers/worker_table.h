
#ifndef PLATFORM_COMPONENTS_WORKERS_WORKER_TABLE_H
#define PLATFORM_COMPONENTS_WORKERS_WORKER_TABLE_H

#include <menabrea/common.h>
#include <menabrea/workers.h>
#include <event_machine.h>

#define MAX_WORKER_NAME_LEN    EM_EO_NAME_LEN  /**< Maximum length of the worker's name */
#define MESSAGE_BUFFER_LENGTH  16              /**< Maximum number of messages buffered by the platform per worker */

typedef enum EWorkerState {
    EWorkerState_Inactive = 0,
    EWorkerState_Deploying,
    EWorkerState_Active,
    EWorkerState_Terminating
} EWorkerState;

typedef struct SWorkerContext {
    TUserInitCallback UserInit;
    TUserLocalInitCallback UserLocalInit;
    TUserLocalExitCallback UserLocalExit;
    TUserExitCallback UserExit;
    TUserHandlerCallback WorkerBody;
    TMessage MessageBuffer[MESSAGE_BUFFER_LENGTH];
    char Name[MAX_WORKER_NAME_LEN];
    int CoreMask;
    bool Parallel;
    bool TerminationRequested;
    EWorkerState State;
    TWorkerId WorkerId;
    em_queue_t Queue;
    em_eo_t Eo;
    TSpinlock Lock;
    void * SharedData;
    void * LocalData[0];
} SWorkerContext;

void WorkerTableInit(TWorkerId nodeId);
void WorkerTableTeardown(void);
SWorkerContext * ReserveWorkerContext(TWorkerId workerId);
void ReleaseWorkerContext(TWorkerId workerId);
SWorkerContext * FetchWorkerContext(TWorkerId workerId);
void MarkDeploymentSuccessful(TWorkerId workerId);
void MarkTeardownInProgress(TWorkerId workerId);
void LockWorkerTableEntry(TWorkerId workerId);
void UnlockWorkerTableEntry(TWorkerId workerId);
void DisableWorkerDeployment(void);

#endif /* PLATFORM_COMPONENTS_WORKERS_WORKER_TABLE_H */
