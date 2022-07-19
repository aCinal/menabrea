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
    TWorkerId workerId = DeploySimpleWorker("time_lord", WORKER_ID_INVALID, GetSharedCoreMask(), WorkerBody);
    if (unlikely(workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the recipient");
        return;
    }

    LogPrint(ELogSeverityLevel_Info, "Creating the timeout message...");
    TMessage message = CreateMessage(0xDEAD, 0);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate the timeout message");
        TerminateWorker(workerId);
        return;
    }

    s_timerIdPtr = (TTimerId *) env_shared_malloc(sizeof(TTimerId));
    AssertTrue(s_timerIdPtr != NULL);

    LogPrint(ELogSeverityLevel_Info, "Creating a one-shot timer...");
    /* One-shot timer to expire one second from now */
    *s_timerIdPtr = CreateTimer(message, workerId, 1 * 1000 * 1000, 0);
    if (unlikely(*s_timerIdPtr == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer");
        DestroyMessage(message);
        TerminateWorker(workerId);
        return;
    }

    LogPrint(ELogSeverityLevel_Info, "Created a one-shot timer with ID: 0x%x", *s_timerIdPtr);
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

    LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received message 0x%x from 0x%x", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));

    LogPrint(ELogSeverityLevel_Info, "Destroying the timer...");
    DestroyTimer(*s_timerIdPtr);

    LogPrint(ELogSeverityLevel_Info, "Timer destroyed");

    LogPrint(ELogSeverityLevel_Info, "Creating another timer just to destroy it immediately...");
    TTimerId timer = CreateTimer(message, GetOwnWorkerId(), 1, 0);

    if (unlikely(timer == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the new timer");
        DestroyMessage(message);

    } else {

        LogPrint(ELogSeverityLevel_Info, "Timer created. Destroying...");
        DestroyTimer(timer);
        LogPrint(ELogSeverityLevel_Info, "New timer destroyed");
    }


    TMessage longRunnerMessage = CreateMessage(0xBEEF, 0);
    if (unlikely(longRunnerMessage == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate the long-runner timer's message");

    } else {

        LogPrint(ELogSeverityLevel_Info, "Creating a long-running timer...");
        /* Create a long-running timer and lose reference to it so that it will
         * need to be cleaned up by the platform */
        TTimerId longRunner = CreateTimer(longRunnerMessage, GetOwnWorkerId(), 600 * 1000 * 1000, 0);
        if (unlikely(longRunner == TIMER_ID_INVALID)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to create the long-running timer");

        } else {

            LogPrint(ELogSeverityLevel_Info, "Timer created");
        }
    }
}
