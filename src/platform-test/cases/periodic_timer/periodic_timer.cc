#include "periodic_timer.hh"
#include <framework/test_runner.hh>
#include <framework/params_parser.hh>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/timing.h>
#include <menabrea/memory.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <chrono>

static TWorkerId s_receiverId = WORKER_ID_INVALID;
static TTimerId s_timerId = TIMER_ID_INVALID;

static constexpr const TMessageId TEST_MESSAGE_ID = 0x6666;

static int ReceiverInit(void * arg);
static void ReceiverExit(void);
static void ReceiverBody(TMessage message);

struct TestPeriodicTimerParams {
    u64 MaxError;
    u64 TimerPeriod;
    u32 MaxMessages;
};

struct TestPeriodicTimerShmem {
    std::chrono::steady_clock::time_point LastTimeout;
    u64 ExpectedPeriod;
    u64 MaxError;
    u32 MessagesReceived;
    u32 MaxMessages;
};

u32 TestPeriodicTimer::GetParamsSize(void) {

    return sizeof(TestPeriodicTimerParams);
}

int TestPeriodicTimer::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["maxError"] = ParamsParser::StructField(offsetof(TestPeriodicTimerParams, MaxError), sizeof(u64), ParamsParser::FieldType::U64);
    paramsLayout["period"] = ParamsParser::StructField(offsetof(TestPeriodicTimerParams, TimerPeriod), sizeof(u64), ParamsParser::FieldType::U64);
    paramsLayout["messages"] = ParamsParser::StructField(offsetof(TestPeriodicTimerParams, MaxMessages), sizeof(u32), ParamsParser::FieldType::U32);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestPeriodicTimer::StartTest(void * args) {

    TestPeriodicTimerParams * params = static_cast<TestPeriodicTimerParams *>(args);

    /* Allocate shared memory for the worker's use*/
    TestPeriodicTimerShmem * shmem = static_cast<TestPeriodicTimerShmem *>(GetMemory(sizeof(TestPeriodicTimerShmem)));
    if (unlikely(shmem == nullptr)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate shared memory for test '%s'", this->GetName());
        return -1;
    }
    new (&shmem->LastTimeout) std::chrono::steady_clock::time_point;
    shmem->ExpectedPeriod = params->TimerPeriod;
    shmem->MaxError = params->MaxError;
    shmem->MessagesReceived = 0;
    shmem->MaxMessages = params->MaxMessages;

    /* Deploy a test worker on a single isolated core */
    SWorkerConfig workerConfig = {
        .Name = "PeriodicTimerTester",
        .InitArg = shmem,
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = 0b10,
        .UserInit = ReceiverInit,
        .UserExit = ReceiverExit,
        .WorkerBody = ReceiverBody
    };
    s_receiverId = DeployWorker(&workerConfig);
    if (unlikely(s_receiverId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the test worker for test '%s'", this->GetName());
        PutMemory(shmem);
        return -1;
    }

    /* Create and arm a periodic timer */
    s_timerId = CreateTimer("TimerUnderTest");
    if (unlikely(s_timerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the periodic timer for test '%s'", this->GetName());
        TerminateWorker(s_receiverId);
        return -1;
    }

    TMessage message = CreateMessage(TEST_MESSAGE_ID, 0);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer message for test '%s'", this->GetName());
        TerminateWorker(s_receiverId);
        DestroyTimer(s_timerId);
        return -1;
    }

    if (unlikely(s_timerId != ArmTimer(s_timerId, params->TimerPeriod, params->TimerPeriod, message, s_receiverId))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to arm the periodic timer in test '%s'", this->GetName());
        TerminateWorker(s_receiverId);
        DestroyTimer(s_timerId);
        DestroyMessage(message);
        return -1;
    }

    return 0;
}

void TestPeriodicTimer::StopTest(void) {

    AssertTrue(s_timerId == DisarmTimer(s_timerId));
    DestroyTimer(s_timerId);
    TerminateWorker(s_receiverId);
}

static int ReceiverInit(void * arg) {

    SetSharedData(arg);
    return 0;
}

static void ReceiverExit(void) {

    TestPeriodicTimerShmem * shmem = static_cast<TestPeriodicTimerShmem *>(GetSharedData());
    /* Destroy the chrono object */
    shmem->LastTimeout.~time_point();
    /* Free the shared memory */
    PutMemory(shmem);
}

static void ReceiverBody(TMessage message) {

    TestPeriodicTimerShmem * shmem = static_cast<TestPeriodicTimerShmem *>(GetSharedData());

    if (unlikely(GetMessageId(message) != TEST_MESSAGE_ID)) {

        LogPrint(ELogSeverityLevel_Warning, "Worker 0x%x received unexpected message 0x%x from 0x%x. Failing the test...", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        TestRunner::ReportTestResult(TestCase::Result::Failure, "Received unexpected message 0x%x from 0x%x", \
            GetMessageId(message), GetMessageSender(message));
        DestroyMessage(message);
        return;
    }

    /* TODO: Consider the overhead of std::chrono (is the time access optimized thanks to VDSO?) */
    std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();

    /* Only check the drift after the first message has been received */
    if (shmem->MessagesReceived > 0) {

        /* Check accuracy */
        u64 observedPeriod = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - shmem->LastTimeout).count();
        if (observedPeriod < shmem->ExpectedPeriod) {

            TestRunner::ReportTestResult(TestCase::Result::Failure, \
                "Observed period %ld less than expected %ld", observedPeriod, shmem->ExpectedPeriod);
            LogPrint(ELogSeverityLevel_Error, \
                "Observed period %ld less than expected %ld", observedPeriod, shmem->ExpectedPeriod);

        } else if (observedPeriod - shmem->ExpectedPeriod > shmem->MaxError) {

            TestRunner::ReportTestResult(TestCase::Result::Failure, \
                "Observed period %ld significantly larger than expected %ld (max error allowed: %ld)", \
                observedPeriod, shmem->ExpectedPeriod, shmem->MaxError);
            LogPrint(ELogSeverityLevel_Error, \
                "Observed period %ld significantly larger than expected %ld (max error allowed: %ld)", \
                observedPeriod, shmem->ExpectedPeriod, shmem->MaxError);
        }
    }

    /* Increment the counter */
    shmem->MessagesReceived++;

    if (shmem->MessagesReceived == shmem->MaxMessages) {

        TestRunner::ReportTestResult(TestCase::Result::Success);
    }

    /* Save current time for the next timeout */
    shmem->LastTimeout = currentTime;

    DestroyMessage(message);
}
