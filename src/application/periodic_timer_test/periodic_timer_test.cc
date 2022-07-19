#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/timing.h>
#include <menabrea/exception.h>

static void WorkerBody(TMessage message);

static TTimerId * s_timerIdPtr = NULL;

extern "C" void ApplicationGlobalInit(void) {

    LogPrint(ELogSeverityLevel_Info, "Deploying the timeout message recipient...");
    TWorkerId workerId = DeploySimpleWorker("periodic_worker", WORKER_ID_INVALID, GetSharedCoreMask(), WorkerBody);
    if (unlikely(workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the recipient");
        return;
    }

    LogPrint(ELogSeverityLevel_Info, "Creating the timeout message...");
    TMessage message = CreateMessage(0xCAFE, 0);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate the timeout message");
        TerminateWorker(workerId);
        return;
    }

    s_timerIdPtr = (TTimerId *) env_shared_malloc(sizeof(TTimerId));
    AssertTrue(s_timerIdPtr != NULL);

    LogPrint(ELogSeverityLevel_Info, "Creating a periodic timer...");
    /* Periodic timer with period 0.5s to expire 3 seconds from now */
    *s_timerIdPtr = CreateTimer(message, workerId, 3 * 1000 * 1000, 500 * 1000);
    if (unlikely(*s_timerIdPtr == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer");
        DestroyMessage(message);
        TerminateWorker(workerId);
        return;
    }

    LogPrint(ELogSeverityLevel_Info, "Created a periodic timer with ID: 0x%x", *s_timerIdPtr);
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

    env_shared_free(s_timerIdPtr);
}

static void WorkerBody(TMessage message) {

    /* Worker deployed only on a single core (single process) */
    static int counter = 0;

    LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received message 0x%x from 0x%x", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
    DestroyMessage(message);

    counter++;

    if (counter == 5) {

        LogPrint(ELogSeverityLevel_Info, "Message received 5 times. Destroying the timer...");
        DestroyTimer(*s_timerIdPtr);

    } else if (counter > 5) {

        LogPrint(ELogSeverityLevel_Info, "Message number %d received. Trying to destroy the timer again...", \
            counter);
        DestroyTimer(*s_timerIdPtr);
    }
}
