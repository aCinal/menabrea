#include "messaging_performance.hh"
#include <menabrea/test/params_parser.hh>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/timing.h>
#include <menabrea/memory.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <chrono>

struct TestMessagingPerformanceParams {
    u64 TimerPeriod;
    u32 PayloadSize;
    u32 Rounds;
    u32 Burst;
    TWorkerId EchoId;
};

struct PerfTestShmem {
    /* TODO: Consider cache line alignment - let each core do their own calculations and then combine them */
    TestMessagingPerformanceParams TestParams;
    /* Test state */
    TAtomic64 MessagesSent;
    u64 MessagesReceived;
    u64 RoundsRemaining;
    /* Performance stats */
    TSpinlock ResultsLock;
    u64 TotalThroughput;
    u64 TotalLatency;
    u64 AverageLatency;
    u64 MaxLatency;
    u64 MinLatency;
};

struct PerfTestPmem {
    TMessage BurstBuffer[0];
};

struct PerfTestMsgHeader {
    u64 Timestamp;
};

static int WorkerInit(void * arg);
static void WorkerLocalInit(int core);
static void WorkerLocalExit(int core);
static void WorkerExit(void);
static void WorkerBody(TMessage message);
static void HandlePerfTestMsg(TMessage message);
static void HandleTimeoutMsg(TMessage message);
static void SendTestMessages(void);
static void MeasurePerformance(TMessage message);
static u64 GetTimestamp(void);

static constexpr const TMessageId MSG_ID_BASE = 0x1000;
static constexpr const TMessageId PERF_TEST_MSG_ID = MSG_ID_BASE;
static constexpr const TMessageId TIMEOUT_MSG_ID = MSG_ID_BASE + 1;

static TWorkerId s_workerId = WORKER_ID_INVALID;
static TTimerId s_timerId = TIMER_ID_INVALID;

u32 TestMessagingPerformance::GetParamsSize(void) {

    return sizeof(TestMessagingPerformanceParams);
}

int TestMessagingPerformance::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["echoId"] = ParamsParser::StructField(offsetof(TestMessagingPerformanceParams, EchoId), sizeof(TWorkerId), ParamsParser::FieldType::U16);
    paramsLayout["payloadSize"] = ParamsParser::StructField(offsetof(TestMessagingPerformanceParams, PayloadSize), sizeof(u32), ParamsParser::FieldType::U32);
    paramsLayout["rounds"] = ParamsParser::StructField(offsetof(TestMessagingPerformanceParams, Rounds), sizeof(u32), ParamsParser::FieldType::U32);
    paramsLayout["burst"] = ParamsParser::StructField(offsetof(TestMessagingPerformanceParams, Burst), sizeof(u32), ParamsParser::FieldType::U32);
    paramsLayout["period"] = ParamsParser::StructField(offsetof(TestMessagingPerformanceParams, TimerPeriod), sizeof(u64), ParamsParser::FieldType::U64);

    if (ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to parse the parameters for test '%s'", this->GetName());
        return -1;
    }

    TestMessagingPerformanceParams * parsed = static_cast<TestMessagingPerformanceParams *>(paramsOut);
    if (parsed->Burst == 0) {

        LogPrint(ELogSeverityLevel_Error, "%s: Burst parameter must be positive", \
            this->GetName());
        return -1;
    }

    if (parsed->TimerPeriod == 0) {

        LogPrint(ELogSeverityLevel_Error, "%s: Timer period must be positive", \
            this->GetName());
        return -1;
    }

    if (parsed->Rounds == 0) {

        LogPrint(ELogSeverityLevel_Error, "%s: Number of rounds must be positive", \
            this->GetName());
        return -1;
    }

    return 0;
}

int TestMessagingPerformance::StartTest(void * args) {

    TestMessagingPerformanceParams * params = static_cast<TestMessagingPerformanceParams *>(args);

    PerfTestShmem * testSharedMemory = \
        static_cast<PerfTestShmem *>(GetRuntimeMemory(sizeof(PerfTestShmem)));
    if (unlikely(testSharedMemory == nullptr)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate shared memory for test '%s'", \
            this->GetName());
        return -1;
    }

    /* Copy the test parameters for the worker's use */
    testSharedMemory->TestParams = *params;
    Atomic64Init(&testSharedMemory->MessagesSent);
    testSharedMemory->MessagesReceived = 0;
    testSharedMemory->RoundsRemaining = params->Rounds;
    SpinlockInit(&testSharedMemory->ResultsLock);
    /* Initialize the performance counters */
    testSharedMemory->TotalThroughput = 0;
    testSharedMemory->TotalLatency = 0;
    testSharedMemory->AverageLatency = 0;
    testSharedMemory->MaxLatency = 0;
    testSharedMemory->MinLatency = static_cast<u64>(-1);

    SWorkerConfig workerConfig = {
        .Name = "PerfTester",
        .InitArg = testSharedMemory,
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetIsolatedCoresMask(),
        .Parallel = false,
        .UserInit = WorkerInit,
        .UserLocalInit = WorkerLocalInit,
        .UserLocalExit = WorkerLocalExit,
        .UserExit = WorkerExit,
        .WorkerBody = WorkerBody
    };
    s_workerId = DeployWorker(&workerConfig);
    if (unlikely(s_workerId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the worker in test '%s'", \
            this->GetName());
        PutRuntimeMemory(testSharedMemory);
        return -1;
    }

    /* The worker synchronously touches the memory in the user init callback,
     * we can safely drop a reference to it here */
    PutRuntimeMemory(testSharedMemory);

    if (unlikely(StartTriggerTimer(params->TimerPeriod))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to start the trigger timer in test '%s'", \
            this->GetName());
        TerminateWorker(s_workerId);
        return -1;
    }

    return 0;
}

void TestMessagingPerformance::StopTest(void) {

    AssertTrue(DisarmTimer(s_timerId) == s_timerId);
    DestroyTimer(s_timerId);
    s_timerId = TIMER_ID_INVALID;
    TerminateWorker(s_workerId);
    s_workerId = WORKER_ID_INVALID;
}

int TestMessagingPerformance::StartTriggerTimer(u64 period) {

    /* Create the timer */
    s_timerId = CreateTimer("PerfTestTimer");
    if (unlikely(s_timerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer in test '%s'", \
            this->GetName());
        return -1;
    }

    /* Create a timeout message */
    TMessage message = CreateMessage(TIMEOUT_MSG_ID, 0);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timeout message in test '%s'", \
            this->GetName());
        DestroyTimer(s_timerId);
        return -1;
    }

    /* Arm the timer */
    if (unlikely(s_timerId != ArmTimer(s_timerId, period, period, message, s_workerId))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to arm the timer with period %ld in test '%s'", \
            period, this->GetName());
        DestroyTimer(s_timerId);
        DestroyMessage(message);
        return -1;
    }

    return 0;
}

static int WorkerInit(void * arg) {

    /* Touch and save the shared memory */
    RefRuntimeMemory(arg);
    SetSharedData(arg);
    return 0;
}

static void WorkerLocalInit(int core) {

    PerfTestShmem * shmem = static_cast<PerfTestShmem *>(GetSharedData());
    u32 burst = shmem->TestParams.Burst;
    /* Allocate private per-core data */
    PerfTestPmem * privateMem = static_cast<PerfTestPmem *>(malloc(sizeof(PerfTestPmem) + sizeof(TMessage) * burst));
    SetLocalData(privateMem);
}

static void WorkerLocalExit(int core) {

    void * pmem = GetLocalData();
    free(pmem);
}

static void WorkerExit(void) {

    void * shmem = GetSharedData();
    PutRuntimeMemory(shmem);
}

static void WorkerBody(TMessage message) {

    switch (GetMessageId(message)) {
    case PERF_TEST_MSG_ID:

        HandlePerfTestMsg(message);
        break;

    case TIMEOUT_MSG_ID:
        HandleTimeoutMsg(message);
        break;

    default:
        LogPrint(ELogSeverityLevel_Error, "Worker 0x%x received unexpected message 0x%x from 0x%x", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        break;
    }

    DestroyMessage(message);
}

static void HandlePerfTestMsg(TMessage message) {

    /* Immediately allow the worker to be scheduled in parallel on a different core */
    EndAtomicContext();

    PerfTestShmem * shmem = static_cast<PerfTestShmem *>(GetSharedData());

    if (likely(GetMessageSender(message) == shmem->TestParams.EchoId)) {

        /* Each message sent has a timestamp - compare them here once
         * they bounce off the echo service */
        MeasurePerformance(message);

    } else {

        LogPrint(ELogSeverityLevel_Error, "Received performance test message 0x%x from an unexpected source 0x%x (expected: 0x%x)", \
            PERF_TEST_MSG_ID, GetMessageSender(message), shmem->TestParams.EchoId);
        TestCase::ReportTestResult(TestCase::Result::Failure, \
            "Received performance test message 0x%x from an unexpected source 0x%x (expected: 0x%x)", \
            PERF_TEST_MSG_ID, GetMessageSender(message), shmem->TestParams.EchoId);
    }
}

static void HandleTimeoutMsg(TMessage message) {

    (void) message;
    PerfTestShmem * shmem = static_cast<PerfTestShmem *>(GetSharedData());

    if (shmem->RoundsRemaining == 0) {

        if (2 * shmem->TotalThroughput < shmem->TotalThroughput) {

            LogPrint(ELogSeverityLevel_Error, \
                "Total throughput counter wrapped when trying to calculate the average");
            TestCase::ReportTestResult(TestCase::Result::Failure, \
                "Total throughput counter wrapped when trying to calculate the average");
            return;
        }

        /* Calculate the average throughput as total throughput divided by
         * the latency */
        u64 totalLatencyMs = shmem->TotalLatency / 1000;
        /* Assume symmetric latency and consider a half of it as a basis
         * for throughput */
        u64 averageThroughput = 2 * shmem->TotalThroughput / totalLatencyMs;

        /* If some messages are missing, do not fail the test immediately - give it time
         * to receive the lost responses or time out naturally */
        if (shmem->MessagesReceived == shmem->TestParams.Rounds * shmem->TestParams.Burst) {

            /* All messages received - report result */
            LogPrint(ELogSeverityLevel_Info, \
                "Latency: avg=%ld, max=%ld, min=%ld [us], throughput=%ld [b/ms]", \
                shmem->AverageLatency, shmem->MaxLatency, shmem->MinLatency,
                averageThroughput);
            TestCase::ReportTestResult(TestCase::Result::Success, \
                "Latency: avg=%ld, max=%ld, min=%ld [us], throughput=%ld [b/ms]", \
                shmem->AverageLatency, shmem->MaxLatency, shmem->MinLatency, \
                averageThroughput);
        }
        return;
    }

    shmem->RoundsRemaining--;

    /* Allow the worker to be scheduled in parallel on a different core */
    EndAtomicContext();
    SendTestMessages();
}

static void SendTestMessages(void) {

    PerfTestShmem * shmem = static_cast<PerfTestShmem *>(GetSharedData());
    PerfTestPmem * pmem = static_cast<PerfTestPmem *>(GetLocalData());

    /* Round up the payload size to at least the size of the internal header if necessary */
    u32 testMessageSize = shmem->TestParams.PayloadSize < sizeof(PerfTestMsgHeader) ? sizeof(PerfTestMsgHeader) : shmem->TestParams.PayloadSize;

    /* Create test messages */
    for (u32 i = 0; i < shmem->TestParams.Burst; i++) {

        TMessage message = CreateMessage(PERF_TEST_MSG_ID, testMessageSize);
        if (unlikely(message == MESSAGE_INVALID)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to create message %d in a burst of %d", \
                i, shmem->TestParams.Burst);
            /* Destroy the messages created thus far */
            for (u32 j = 0; j < i; j++) {

                DestroyMessage(pmem->BurstBuffer[j]);
            }
            /* Report failure and return */
            TestCase::ReportTestResult(TestCase::Result::Failure, \
                "Failed to create message %d in a burst of %d", \
                i, shmem->TestParams.Burst);
            return;
        }

        pmem->BurstBuffer[i] = message;
    }

    /* Send the messages in a single burst */
    for (u32 i = 0; i < shmem->TestParams.Burst; i++) {

        Atomic64Inc(&shmem->MessagesSent);
        PerfTestMsgHeader * hdr = static_cast<PerfTestMsgHeader *>(GetMessagePayload(pmem->BurstBuffer[i]));
        hdr->Timestamp = GetTimestamp();
        SendMessage(pmem->BurstBuffer[i], shmem->TestParams.EchoId);
    }
}

static void MeasurePerformance(TMessage message) {

    /* Get current time as early as possible */
    u64 currentTime = GetTimestamp();

    PerfTestShmem * shmem = static_cast<PerfTestShmem *>(GetSharedData());
    PerfTestMsgHeader * hdr = static_cast<PerfTestMsgHeader *>(GetMessagePayload(message));
    if (unlikely(currentTime < hdr->Timestamp)) {

        LogPrint(ELogSeverityLevel_Error, "Clock's counter wrapped");
        TestCase::ReportTestResult(TestCase::Result::Failure, \
            "Clock's counter wrapped");
        return;
    }

    /* Calculate the time delta */
    u64 latency = currentTime - hdr->Timestamp;

    SpinlockAcquire(&shmem->ResultsLock);
    /* Assert that latency and throughput are monotonically increasing without
     * wrapping */
    u64 totalLatency = shmem->TotalLatency + latency;
    if (unlikely(totalLatency < shmem->TotalLatency)) {

        SpinlockRelease(&shmem->ResultsLock);
        LogPrint(ELogSeverityLevel_Error, "Total latency counter wrapped");
        TestCase::ReportTestResult(TestCase::Result::Failure, \
            "Total latency counter wrapped");
        return;
    }

    u64 totalThroughput = shmem->TotalThroughput + GetMessagePayloadSize(message);
    if (unlikely(totalThroughput < shmem->TotalThroughput)) {

        SpinlockRelease(&shmem->ResultsLock);
        LogPrint(ELogSeverityLevel_Error, "Total throughput counter wrapped");
        TestCase::ReportTestResult(TestCase::Result::Failure, \
            "Total throughput counter wrapped");
        return;
    }

    /* Update the counters */
    shmem->MessagesReceived++;
    u64 messagesReceived = shmem->MessagesReceived;

    shmem->TotalThroughput = totalThroughput;
    shmem->TotalLatency = totalLatency;
    shmem->AverageLatency = shmem->TotalLatency / shmem->MessagesReceived;
    shmem->MaxLatency = MAX(shmem->MaxLatency, latency);
    shmem->MinLatency = MIN(shmem->MinLatency, latency);
    SpinlockRelease(&shmem->ResultsLock);

    if (messagesReceived % shmem->TestParams.Burst == 0) {

        /* Extend the timeout every time a full burst is received */
        TestCase::ExtendTimeout();
    }
}

static u64 GetTimestamp(void) {

    auto timePoint = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    auto sinceEpoch = timePoint.time_since_epoch();
    return sinceEpoch.count();
}
