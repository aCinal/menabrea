#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>

static int WorkerInit(void);
static void WorkerLocalInit(int core);
static void WorkerBody(TMessage message);

extern "C" void ApplicationGlobalInit(void) {

    SWorkerConfig config = {
        .Name = "kamikaze",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = 0b1111,
        .Parallel = false,
        .UserInit = WorkerInit,
        .WorkerBody = WorkerBody
    };

    (void) DeployWorker(&config);
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

}

static int WorkerInit(void) {

    TWorkerId self = GetOwnWorkerId();

    LogPrint(ELogSeverityLevel_Info, "%s() called for worker 0x%x", \
        __FUNCTION__, self);

    /* Spawn a new worker */
    SWorkerConfig config = {
        .Name = "kamikaze",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = 0b1111,
        .Parallel = false,
        .UserLocalInit = WorkerLocalInit,
        .WorkerBody = WorkerBody
    };
    TWorkerId newWorker = DeployWorker(&config);
    if (newWorker != WORKER_ID_INVALID) {

        LogPrint(ELogSeverityLevel_Info, "Deployed worker 0x%x", newWorker);
    }

    /* Terminate self during init using WORKER_ID_INVALID */
    TerminateWorker(WORKER_ID_INVALID);

    /* Try continuing using platform APIs after termination */
    TMessage message = CreateMessage(0xDEAD, 0);
    if (message != MESSAGE_INVALID) {

        /* Send a message back to self, it should be dropped as termination
         * has already been requested */
        SendMessage(message, self);

    } else {

        LogPrint(ELogSeverityLevel_Error, "Failed to create a message");
    }

    return 0;
}

static void WorkerLocalInit(int core) {

    TWorkerId self = GetOwnWorkerId();
    LogPrint(ELogSeverityLevel_Info, "%s() called for worker 0x%x on core %d", \
        __FUNCTION__, self, core);

    /* Request termination during local inits on all cores. The call should fail
     * gracefully. */
    TerminateWorker(self);
}

static void WorkerBody(TMessage message) {

    LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received message 0x%x from 0x%x", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
    DestroyMessage(message);
}
