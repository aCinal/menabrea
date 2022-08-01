#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/timing.h>
#include <menabrea/exception.h>

static void WorkerBody(TMessage message);

#define APP_TIMEOUT_MSG_ID  0xDEAD
struct AppTimeoutMsg {
    TTimerId TimerId;
    bool Periodic;
};

static void CreateOneShot(TWorkerId receiver);
static void CreatePeriodic(TWorkerId receiver);
static void CreateLongRunner(TWorkerId receiver);
static void CreateAndCancel(TWorkerId receiver);

extern "C" void ApplicationGlobalInit(void) {

    LogPrint(ELogSeverityLevel_Info, "Deploying the timeout message recipient...");
    TWorkerId workerId = DeploySimpleWorker("time_lord", WORKER_ID_INVALID, GetSharedCoreMask(), WorkerBody);
    if (unlikely(workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the recipient");
        return;
    }

    CreateOneShot(workerId);
    CreatePeriodic(workerId);
    CreateLongRunner(workerId);
    CreateAndCancel(workerId);
}

static void CreateOneShot(TWorkerId receiver) {

    LogPrint(ELogSeverityLevel_Info, "In %s()", __FUNCTION__);
    /* Create a timeout message */
    TMessage message = CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg));
    AssertTrue(message != MESSAGE_INVALID);
    /* Create a timer */
    TTimerId timerId = CreateTimer("one-shot");
    AssertTrue(timerId != TIMER_ID_INVALID);
    /* Save the timer ID in the message payload */
    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    payload->TimerId = timerId;
    payload->Periodic = false;
    /* Arm the timer and assert success */
    TTimerId ret = ArmTimer(timerId, 5 * 1000, 0, message, receiver);
    AssertTrue(ret == timerId);
}

static void CreatePeriodic(TWorkerId receiver) {

    LogPrint(ELogSeverityLevel_Info, "In %s()", __FUNCTION__);
    /* Create a timeout message */
    TMessage message = CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg));
    AssertTrue(message != MESSAGE_INVALID);
    /* Create a timer */
    TTimerId timerId = CreateTimer("periodic");
    AssertTrue(timerId != TIMER_ID_INVALID);
    /* Save the timer ID in the message payload */
    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    payload->TimerId = timerId;
    payload->Periodic = true;
    /* Arm the timer and assert success */
    TTimerId ret = ArmTimer(timerId, 10 * 1000, 1 * 1000 * 1000, message, receiver);
    AssertTrue(ret == timerId);
}

static void CreateLongRunner(TWorkerId receiver) {

    LogPrint(ELogSeverityLevel_Info, "In %s()", __FUNCTION__);
    /* Create a timeout message */
    TMessage message = CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg));
    AssertTrue(message != MESSAGE_INVALID);
    /* Create a timer */
    TTimerId timerId = CreateTimer("long-runner");
    AssertTrue(timerId != TIMER_ID_INVALID);
    /* Save the timer ID in the message payload */
    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    payload->TimerId = timerId;
    payload->Periodic = false;
    /* Arm the timer and assert success */
    TTimerId ret = ArmTimer(timerId, 600 * 1000 * 1000, 0, message, receiver);
    AssertTrue(ret == timerId);
}

static void CreateAndCancel(TWorkerId receiver) {

    LogPrint(ELogSeverityLevel_Info, "In %s()", __FUNCTION__);
    /* Create a timeout message */
    TMessage message = CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg));
    AssertTrue(message != MESSAGE_INVALID);
    /* Create a timer */
    TTimerId timerId = CreateTimer("short-lived");
    AssertTrue(timerId != TIMER_ID_INVALID);
    /* Save the timer ID in the message payload */
    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    payload->TimerId = timerId;
    payload->Periodic = false;
    /* Arm the timer and assert success */
    TTimerId ret = ArmTimer(timerId, 20 * 1000 * 1000, 0, message, receiver);
    AssertTrue(ret == timerId);
    AssertTrue(timerId == DisarmTimer(timerId));
    DestroyTimer(timerId);
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

}

static void WorkerBody(TMessage message) {

    /* The worker is deployed on the shared core only - use a static variable */
    static int periodicTimeouts = 0;

    LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received message 0x%x from 0x%x", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));

    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    if (payload->Periodic) {

        periodicTimeouts++;

        if (periodicTimeouts == 5) {

            /* Cancel and destroy the timer after 5 periods */
            if (payload->TimerId == DisarmTimer(payload->TimerId)) {

                DestroyTimer(payload->TimerId);

            } else {

                LogPrint(ELogSeverityLevel_Error, "Failed to disarm a periodic timer 0x%x", \
                    payload->TimerId);
            }
        } else {

            LogPrint(ELogSeverityLevel_Info, "Received periodic timeout number %d", periodicTimeouts);
        }

    } else {

        /* One-shot timer - destroy immediately */
        DestroyTimer(payload->TimerId);
    }

    DestroyMessage(message);
}
