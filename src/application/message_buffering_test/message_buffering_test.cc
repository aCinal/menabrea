#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/cores.h>

static void ProducerBody(TMessage message);
static void ConsumerBody(TMessage message);

extern "C" void ApplicationGlobalInit(void) {

    TWorkerId workerId = DeploySimpleWorker("producer", WORKER_ID_INVALID, GetSharedCoreMask(), ProducerBody);
    if (workerId == WORKER_ID_INVALID) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the producer");
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

static void ProducerBody(TMessage message) {

    TWorkerId self = GetOwnWorkerId();
    LogPrint(ELogSeverityLevel_Info, "Producer activated. Spawning a consumer worker and overloading it with messages...");

    int messageCount = 32;
    /* Deploy a new worker on the same core - its local startup will have no way of completing on
     * this core and in the meantime we shall send 32 messages to it... */
    TWorkerId workerId = DeploySimpleWorker("consumer", WORKER_ID_INVALID, GetSharedCoreMask(), ConsumerBody);

    if (unlikely(workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to spawn the consumer. Terminating the producer...");
        TerminateWorker(self);
        return;
    }

    while (messageCount--) {

        TMessage newMessage = CreateMessage(0xCAFE, 0);
        if (unlikely(newMessage == MESSAGE_INVALID)) {

            LogPrint(ELogSeverityLevel_Info, "Failed to create the message. Producer giving up...");
            TerminateWorker(self);
            return;
        }

        SendMessage(newMessage, workerId);
    }

    TerminateWorker(self);
}

static void ConsumerBody(TMessage message) {

    LogPrint(ELogSeverityLevel_Info, "Consumer 0x%x received message 0x%x from 0x%x", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
    DestroyMessage(message);
}
