#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>

#define ECHO_LOCAL_ID  0x0700

static void EchoService(TMessage message);

extern "C" void ApplicationGlobalInit(void) {

    TWorkerId workerId = DeploySimpleParallelWorker("EchoService", ECHO_LOCAL_ID, GetAllCoresMask(), EchoService);
    if (unlikely(WORKER_ID_INVALID == workerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the echo service worker");
    }
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

    TerminateWorker(MakeWorkerId(GetOwnNodeId(), ECHO_LOCAL_ID));
}

static void EchoService(TMessage message) {

    TWorkerId sender = GetMessageSender(message);
    /* Return to sender */
    SendMessage(message, sender);
}
