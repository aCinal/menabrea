#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

static void WorkerBody(TMessage message);

extern "C" void ApplicationGlobalInit(void) {

    TWorkerId workerId = DeploySimpleWorker("phoenix_worker", WORKER_ID_INVALID, 0b1111, WorkerBody);
    if (workerId == WORKER_ID_INVALID) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the initial worker");
        return;
    }

    LogPrint(ELogSeverityLevel_Debug, "Successfully deployed worker 0x%x. Creating a trigger message...", workerId);
    TMessage message = CreateMessage(0xDEAD, 0);
    if (message == MESSAGE_INVALID) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the initial message");
        return;
    }

    LogPrint(ELogSeverityLevel_Debug, "Trigger message successfully created. Sending to 0x%x...", workerId);
    SendMessage(message, workerId);
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
    LogPrint(ELogSeverityLevel_Info, "%s(): Worker 0x%x received message 0x%x from 0x%x. Spawning a copy and terminating...", \
        __FUNCTION__, self, GetMessageId(message), GetMessageSender(message));

    DestroyMessage(message);

    TMessage trigger = CreateMessage(0xDEAD, 0);
    if (unlikely(trigger == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the message. Killing self immediately...");
        TerminateWorker(self);
        return;
    }

    TWorkerId nextWorker = DeploySimpleWorker("phoenix_worker", WORKER_ID_INVALID, 0b1111, WorkerBody);
    if (likely(nextWorker != WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Info, "Spawned new worker with ID 0x%x. Sending a trigger message...", nextWorker);
        /* Send a trigger message to the new incarnation */
        SendMessage(trigger, nextWorker);

    } else {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the next incarnation");
    }

    /* Kill self */
    TerminateWorker(self);
}
