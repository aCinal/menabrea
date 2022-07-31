#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/timing.h>
#include <menabrea/exception.h>
#include <time.h>
#include <stdlib.h>

static void WorkerBody(TMessage message);

#define APP_TIMEOUT_MSG_ID  0xCAFE
struct AppTimeoutMsg {
    TTimerId TimerId;
};

static void CreatePeriodic(TWorkerId receiver);

extern "C" void ApplicationGlobalInit(void) {

    srand(time(NULL));

    LogPrint(ELogSeverityLevel_Info, "Deploying the timeout message recipient...");
    TWorkerId workerId = DeploySimpleWorker("blue_period", WORKER_ID_INVALID, GetSharedCoreMask(), WorkerBody);
    if (unlikely(workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the recipient");
        return;
    }

    CreatePeriodic(workerId);
}

static void CreatePeriodic(TWorkerId receiver) {

    LogPrint(ELogSeverityLevel_Info, "In %s()", __FUNCTION__);
    /* Create a timeout message */
    TMessage message = CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg));
    AssertTrue(message != MESSAGE_INVALID);
    /* Create a timer */
    TTimerId timerId = CreateTimer();
    AssertTrue(timerId != TIMER_ID_INVALID);
    /* Save the timer ID in the message payload */
    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    payload->TimerId = timerId;
    /* Arm the timer and assert success */
    TTimerId ret = ArmTimer(timerId, 10 * 1000, 500 * 1000, message, receiver);
    AssertTrue(ret == timerId);
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

    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    AssertTrue(payload->TimerId == DisarmTimer(payload->TimerId));
    u64 newPeriod = (rand() % 1000 + 1) * 1000;
    LogPrint(ELogSeverityLevel_Info, "Rearming timer 0x%x with period %lu", \
        payload->TimerId, newPeriod);
    /* Rearm the timer with the same message */
    AssertTrue(payload->TimerId == ArmTimer(payload->TimerId, newPeriod, newPeriod, message, GetOwnWorkerId()));
}
