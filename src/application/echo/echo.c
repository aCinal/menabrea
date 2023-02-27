#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>

#define ECHO_LOCAL_ID  0x0700

static void EchoService(TMessage message);

APPLICATION_GLOBAL_INIT() {

    TWorkerId workerId = DeploySimpleParallelWorker("EchoService", ECHO_LOCAL_ID, GetAllCoresMask(), EchoService);
    if (unlikely(WORKER_ID_INVALID == workerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the echo service worker");
    }
}

APPLICATION_LOCAL_INIT(core) {

    (void) core;
}

APPLICATION_LOCAL_EXIT(core) {

    (void) core;
}

APPLICATION_GLOBAL_EXIT() {

    TerminateWorker(MakeWorkerId(GetOwnNodeId(), ECHO_LOCAL_ID));
}

static void EchoService(TMessage message) {

    TWorkerId sender = GetMessageSender(message);
    /* Return to sender */
    SendMessage(message, sender);
}
