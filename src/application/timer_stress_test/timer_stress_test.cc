#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/timing.h>
#include <menabrea/exception.h>
#include <menabrea/cores.h>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <algorithm>
#include <vector>

static void WorkerBody(TMessage message);

#define CONSTANT_DELAY          10 * 1000 * 1000
#define MAX_TIMEOUTS_PER_TIMER  512
#define TIMER_PERIOD            100
#define TIMER_COUNT             32
#define APP_TIMEOUT_MSG_ID      0xDEAD
struct AppTimeoutMsg {
    TTimerId TimerId;
};

/* Worker only deployed on core 0 - use a static variable */
static std::map<TTimerId, u32> s_timeouts;

static void DoArmDisarmSequence(TWorkerId receiver, u32 count);
static void CreatePeriodicTimers(TWorkerId receiver, u32 count, u64 period);

extern "C" void ApplicationGlobalInit(void) {

    srand(time(nullptr));

    LogPrint(ELogSeverityLevel_Info, "Deploying the timeout message recipient...");
    TWorkerId workerId = DeploySimpleWorker("time_lord", WORKER_ID_INVALID, GetSharedCoreMask(), WorkerBody);
    if (unlikely(workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the recipient");
        return;
    }

    DoArmDisarmSequence(workerId, 256);
    CreatePeriodicTimers(workerId, TIMER_COUNT, TIMER_PERIOD);
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    if (core == 0) {

        for (auto & [key, value] : s_timeouts) {

            LogPrint(ELogSeverityLevel_Info, "    Received %d timeout messages for timer 0x%x", \
                value, key);
        }
    }
}

extern "C" void ApplicationGlobalExit(void) {

}

static void CreatePeriodicTimers(TWorkerId receiver, u32 count, u64 period) {

    LogPrint(ELogSeverityLevel_Info, "In %s()", __FUNCTION__);
    for (u32 i = 0; i < count; i++) {

        /* Create a timeout message */
        TMessage message = CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg));
        AssertTrue(message != MESSAGE_INVALID);

        /* Create a timer */
        char name[16];
        (void) snprintf(name, sizeof(name), "periodic_%u", i);
        TTimerId timerId = CreateTimer(name);
        AssertTrue(timerId != TIMER_ID_INVALID);

        /* Save the timer ID in the message payload */
        AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
        payload->TimerId = timerId;

        /* Arm the timer and assert success */
        TTimerId ret = ArmTimer(timerId, (rand() % 100) * 1000 + CONSTANT_DELAY, period, message, receiver);
        AssertTrue(ret == timerId);
    }
}

static void DoArmDisarmSequence(TWorkerId receiver, u32 count) {

    /* Create 'count' messages */
    std::vector<TMessage> messages;
    for (u32 i = 0; i < count; i++) {

        messages.push_back(CreateMessage(APP_TIMEOUT_MSG_ID, sizeof(AppTimeoutMsg)));
        AssertTrue(messages.back() != MESSAGE_INVALID);
    }

    /* Create a timer */
    TTimerId timerId = CreateTimer("unstable_timer");
    AssertTrue(timerId != TIMER_ID_INVALID);

    std::for_each(
        messages.begin(),
        messages.end(),
        [timerId](TMessage message) {

            /* Save the timer ID in the message payload */
            AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
            payload->TimerId = timerId;
        }
    );

    for (u32 i = 0; i < count; i++) {

        /* Arm the timer and assert success */
        TTimerId ret = ArmTimer(timerId, 1000, 1000, messages.back(), receiver);
        messages.pop_back();
        AssertTrue(ret == timerId);
        AssertTrue(timerId == DisarmTimer(timerId));
    }
}

static void WorkerBody(TMessage message) {

    AppTimeoutMsg * payload = static_cast<AppTimeoutMsg *>(GetMessagePayload(message));
    s_timeouts[payload->TimerId]++;

    if (s_timeouts[payload->TimerId] == MAX_TIMEOUTS_PER_TIMER) {

        AssertTrue(payload->TimerId == DisarmTimer(payload->TimerId));
        DestroyTimer(payload->TimerId);
        LogPrint(ELogSeverityLevel_Info, "Destroyed timer 0x%x", payload->TimerId);
    }

    DestroyMessage(message);
}
