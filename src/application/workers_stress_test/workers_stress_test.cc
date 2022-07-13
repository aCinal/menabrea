#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <stdlib.h>
#include <time.h>

static void WorkerBody(TMessage message);
static TWorkerId GetRandomWorkerId(void);
static void MasterLocalInit(int core);
static void WorkerLocalInit(int core);

extern "C" void ApplicationGlobalInit(void) {

    srand(time(NULL));

    SWorkerConfig masterConfig = {
        .Name = "MasterTrigger",
        .WorkerId = 0xFF,
        .CoreMask = 0b0001,
        .Parallel = false,
        .UserLocalInit = MasterLocalInit,
        .WorkerBody = WorkerBody
    };
    DeployWorker(&masterConfig);
}

extern "C" void ApplicationLocalInit(int core) {

    LogPrint(ELogSeverityLevel_Info, "%s() called on core %d", __FUNCTION__, core);
}

extern "C" void ApplicationLocalExit(int core) {

    LogPrint(ELogSeverityLevel_Info, "%s() called on core %d", __FUNCTION__, core);
}

extern "C" void ApplicationGlobalExit(void) {

    LogPrint(ELogSeverityLevel_Info, "%s() called", __FUNCTION__);
}

static void WorkerBody(TMessage message) {

    TWorkerId self = GetOwnWorkerId();
    DestroyMessage(message);

    /* Kill yourself */
    TerminateWorker(self);
}

static TWorkerId GetRandomWorkerId(void) {

    return rand() % MAX_WORKER_COUNT;
}

static void MasterLocalInit(int core) {

    LogPrint(ELogSeverityLevel_Info, "%s() called on core %d. Spawning a worker with dynamic ID...", \
        __FUNCTION__, core);

    char name[64];
    (void) snprintf(name, sizeof(name), "Worker_%u", rand() + MAX_WORKER_COUNT);

    SWorkerConfig workerConfig = {
        .Name = name,
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = 0b1111,
        .Parallel = false,
        .UserLocalInit = WorkerLocalInit,
        .WorkerBody = WorkerBody
    };
    DeployWorker(&workerConfig);
}

static void WorkerLocalInit(int core) {

    char name[64];
    (void) snprintf(name, sizeof(name), "Worker_%u", rand() + MAX_WORKER_COUNT);
    /* Deploy a new worker */
    SWorkerConfig workerConfig = {
        .Name = name,
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = 0b1111,
        .Parallel = true,
        .UserLocalInit = WorkerLocalInit,
        .WorkerBody = WorkerBody
    };
    DeployWorker(&workerConfig);

    /* Kill two random workers */
    TMessage killMessage1 = CreateMessage(0xDEAD, 0);
    TMessage killMessage2 = CreateMessage(0xDEAD, 0);
    if (likely(killMessage1 != MESSAGE_INVALID)) {

        TWorkerId target = GetRandomWorkerId();
        SendMessage(killMessage1, target);

    } else {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the kill message 1");
    }

    if (likely(killMessage2 != MESSAGE_INVALID)) {

        TWorkerId target = GetRandomWorkerId();
        SendMessage(killMessage2, target);

    } else {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the kill message 2");
    }
}
