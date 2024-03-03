#include "oneshot_timer.hh"
#include <menabrea/test/params_parser.hh>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/timing.h>
#include <menabrea/memory.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/cores.h>
#include <chrono>

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;

static TWorkerId s_receiverId = WORKER_ID_INVALID;
static TTimerId s_timerId = TIMER_ID_INVALID;

static constexpr const TMessageId TEST_MESSAGE_ID = 0x6666;

static int ReceiverInit(void * arg);
static void ReceiverExit(void);
static void ReceiverBody(TMessage message);
static int ArmTimerUnderTest(TTimerId timerId, u64 expirationTime, TWorkerId receiverId, TimePoint & timeWhenArmed);

struct TestOneshotTimerParams {
    u64 MaxError;
    u64 ExpirationTime;
    u32 MaxMessages;
};

struct TestOneshotTimerShmem {
    TimePoint TimeWhenArmed;
    u64 ExpirationTime;
    u64 MaxError;
    u32 MessagesReceived;
    u32 MaxMessages;
    TTimerId TimerId;
};

u32 TestOneshotTimer::GetParamsSize(void) {

    return sizeof(TestOneshotTimerParams);
}

int TestOneshotTimer::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["maxError"] = ParamsParser::StructField(offsetof(TestOneshotTimerParams, MaxError), sizeof(u64), ParamsParser::FieldType::U64);
    paramsLayout["expiration"] = ParamsParser::StructField(offsetof(TestOneshotTimerParams, ExpirationTime), sizeof(u64), ParamsParser::FieldType::U64);
    paramsLayout["messages"] = ParamsParser::StructField(offsetof(TestOneshotTimerParams, MaxMessages), sizeof(u32), ParamsParser::FieldType::U32);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestOneshotTimer::StartTest(void * args) {

    TestOneshotTimerParams * params = static_cast<TestOneshotTimerParams *>(args);

    /* Allocate shared memory for the worker's use*/
    TestOneshotTimerShmem * shmem = \
        static_cast<TestOneshotTimerShmem *>(GetRuntimeMemory(sizeof(TestOneshotTimerShmem)));
    if (unlikely(shmem == nullptr)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate shared memory for test '%s'", this->GetName());
        return -1;
    }
    new (&shmem->TimeWhenArmed) TimePoint;
    shmem->ExpirationTime = params->ExpirationTime;
    shmem->MaxError = params->MaxError;
    shmem->MessagesReceived = 0;
    shmem->MaxMessages = params->MaxMessages;

    /* Deploy a test worker on the isolated cores */
    SWorkerConfig workerConfig = {
        .Name = "OneshotTimerTester",
        .InitArg = shmem,
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetIsolatedCoresMask(),
        .UserInit = ReceiverInit,
        .UserExit = ReceiverExit,
        .WorkerBody = ReceiverBody
    };
    s_receiverId = DeployWorker(&workerConfig);
    if (unlikely(s_receiverId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the test worker for test '%s'", this->GetName());
        PutRuntimeMemory(shmem);
        return -1;
    }

    /* Create and arm a oneshot timer */
    shmem->TimerId = s_timerId = CreateTimer("TimerUnderTest");
    if (unlikely(s_timerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer for test '%s'", this->GetName());
        TerminateWorker(s_receiverId);
        return -1;
    }

    if (unlikely(ArmTimerUnderTest(s_timerId, params->ExpirationTime, s_receiverId, shmem->TimeWhenArmed))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to arm timer under test for the first time in test '%s'", \
            this->GetName());
        TerminateWorker(s_receiverId);
        DestroyTimer(s_timerId);
        return -1;
    }

    return 0;
}

void TestOneshotTimer::StopTest(void) {

    AssertTrue(s_timerId == DisarmTimer(s_timerId));
    DestroyTimer(s_timerId);
    TerminateWorker(s_receiverId);
}

static int ReceiverInit(void * arg) {

    SetSharedData(arg);
    return 0;
}

static void ReceiverExit(void) {

    TestOneshotTimerShmem * shmem = static_cast<TestOneshotTimerShmem *>(GetSharedData());
    /* Destroy the chrono object */
    shmem->TimeWhenArmed.~time_point();
    /* Free the shared memory */
    PutRuntimeMemory(shmem);
}

static void ReceiverBody(TMessage message) {

    /* Get the current time as early as possible */
    TimePoint currentTime = SteadyClock::now();

    TestOneshotTimerShmem * shmem = static_cast<TestOneshotTimerShmem *>(GetSharedData());

    if (unlikely(GetMessageId(message) != TEST_MESSAGE_ID)) {

        LogPrint(ELogSeverityLevel_Warning, "Worker 0x%x received unexpected message 0x%x from 0x%x. Failing the test...", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        TestCase::ReportTestResult(TestCase::Result::Failure, "Received unexpected message 0x%x from 0x%x", \
            GetMessageId(message), GetMessageSender(message));
        DestroyMessage(message);
        return;
    }

    /* Only check the drift after the first message has been received */
    if (shmem->MessagesReceived > 0) {

        /* Check accuracy */
        u64 expirationTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - shmem->TimeWhenArmed).count();
        if (expirationTime > shmem->ExpirationTime + shmem->MaxError or expirationTime < std::min(0UL, shmem->ExpirationTime - shmem->MaxError)) {

            TestCase::ReportTestResult(TestCase::Result::Failure, \
                "Observed expiration time %ld significantly different from expected %ld (max error allowed: %ld)", \
                expirationTime, shmem->ExpirationTime, shmem->MaxError);
            LogPrint(ELogSeverityLevel_Error, \
                "Observed expiration time %ld significantly different from expected %ld (max error allowed: %ld)", \
                expirationTime, shmem->ExpirationTime, shmem->MaxError);
        }
    }

    /* Increment the counter */
    shmem->MessagesReceived++;

    if (shmem->MessagesReceived == shmem->MaxMessages) {

        TestCase::ReportTestResult(TestCase::Result::Success);
    }

    DestroyMessage(message);

    if (unlikely(ArmTimerUnderTest(shmem->TimerId, shmem->ExpirationTime, GetOwnWorkerId(), shmem->TimeWhenArmed))) {

        TestCase::ReportTestResult(TestCase::Result::Failure, \
            "Failed to rearm the timer after %d iteration(s)", \
            shmem->MessagesReceived);
        LogPrint(ELogSeverityLevel_Error, "Failed to rearm timer 0x%x after %d iteration(s)", \
            s_timerId, shmem->MessagesReceived);
    }
}

static int ArmTimerUnderTest(TTimerId timerId, u64 expirationTime, TWorkerId receiverId, TimePoint & timeWhenArmed) {

    AssertTrue(timerId != TIMER_ID_INVALID);
    AssertTrue(receiverId != WORKER_ID_INVALID);
    TMessage message = CreateMessage(TEST_MESSAGE_ID, 0);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer message");
        return -1;
    }

    /* Store the time in shared memory instead to avoid constructing and destroying it every time a new message
     * is created and sent. Save the time before actually arming the timer to avoid a race condition. */
    timeWhenArmed = SteadyClock::now();
    if (unlikely(timerId != ArmTimer(timerId, expirationTime, 0, message, receiverId))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to arm the timer under test");
        DestroyMessage(message);
        return -1;
    }

    return 0;
}
