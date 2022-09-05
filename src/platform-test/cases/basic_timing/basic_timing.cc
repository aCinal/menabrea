#include "basic_timing.hh"
#include <framework/test_runner.hh>
#include <framework/params_parser.hh>
#include <menabrea/workers.h>
#include <menabrea/cores.h>
#include <menabrea/messaging.h>
#include <menabrea/timing.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <vector>
#include <algorithm>

static TWorkerId s_receiverId = WORKER_ID_INVALID;
static TTimerId s_timerId = TIMER_ID_INVALID;

static constexpr const u64 SUBCASE_0_EXPIRATION_TIME = 5 * 1000;
static constexpr const u32 SUBCASE_1_MESSAGE_COUNT = 20;
static constexpr const u64 SUBCASE_1_PERIOD = 1000;
static constexpr const TMessageId TEST_MESSAGE_ID = 0x6666;

static void ReceiverBody(TMessage message);

struct TestBasicTimingParams {
    u32 Subcase;
};

u32 TestBasicTiming::GetParamsSize(void) {

    return sizeof(TestBasicTimingParams);
}

int TestBasicTiming::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["subcase"] = ParamsParser::StructField(offsetof(TestBasicTimingParams, Subcase), sizeof(u32), ParamsParser::FieldType::U32);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestBasicTiming::StartTest(void * args) {

    TestBasicTimingParams * params = static_cast<TestBasicTimingParams *>(args);

    /* Deploy a test worker on the current core */
    s_receiverId = DeploySimpleWorker("BasicTimingTester", WORKER_ID_INVALID, GetSharedCoreMask(), ReceiverBody);
    if (unlikely(s_receiverId == WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the test worker for test '%s'", this->GetName());
        return -1;
    }

    switch (params->Subcase) {
    case 0:
        return RunSubcase0();

    case 1:
        return RunSubcase1();

    case 2:
        return RunSubcase2();

    default:
        LogPrint(ELogSeverityLevel_Warning, "Invalid subcase %d for test case '%s'", \
            params->Subcase, this->GetName());
        return -1;
    }
}

void TestBasicTiming::StopTest(void) {

    TerminateWorker(s_receiverId);
    if (TIMER_ID_INVALID != s_timerId) {

        AssertTrue(s_timerId == DisarmTimer(s_timerId));
        DestroyTimer(s_timerId);
        s_timerId = TIMER_ID_INVALID;
    }
}

int TestBasicTiming::RunSubcase0(void) {

    /* Create a timeout message */
    TMessage message = CreateMessage(TEST_MESSAGE_ID, 0);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate message for test case '%s'", \
            this->GetName());
        return -1;
    }
    /* Create a timer */
    s_timerId = CreateTimer("OneShot");
    if (unlikely(s_timerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create timer for test case '%s'", \
            this->GetName());
        DestroyMessage(message);
        return -1;
    }
    /* Arm the timer and assert success */
    TTimerId ret = ArmTimer(s_timerId, SUBCASE_0_EXPIRATION_TIME, 0, message, s_receiverId);
    if (unlikely(ret != s_timerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to arm the timer for test case '%s'", \
            this->GetName());
        DestroyMessage(message);
        DestroyTimer(s_timerId);
        s_timerId = TIMER_ID_INVALID;
        return -1;
    }

    /* Test started successfully - test status will be reported asynchronously
     * by the receiver when the timer expires */
    return 0;
}

int TestBasicTiming::RunSubcase1(void) {

    /* Create 'SUBCASE_1_MESSAGE_COUNT' messages */
    std::vector<TMessage> messages;
    for (u32 i = 0; i < SUBCASE_1_MESSAGE_COUNT; i++) {

        messages.push_back(CreateMessage(TEST_MESSAGE_ID, 0));
        if (unlikely(messages.back() == MESSAGE_INVALID)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to create message number %d for test case '%s'", \
                i, this->GetName());
            std::for_each(
                messages.begin(),
                messages.end(),
                [](TMessage message) {
                    DestroyMessage(message);
                }
            );
            return -1;
        }
    }

    /* Create a timer */
    TTimerId timerId = CreateTimer("UnstableTimer");
    if (unlikely(timerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer for test case '%s'", this->GetName());
        std::for_each(
            messages.begin(),
            messages.end(),
            [](TMessage message) {
                DestroyMessage(message);
            }
        );
        return -1;
    }

    for (u32 i = 0; i < SUBCASE_1_MESSAGE_COUNT; i++) {

        /* Arm the timer and assert success */
        TTimerId ret = ArmTimer(timerId, SUBCASE_1_PERIOD, SUBCASE_1_PERIOD, messages.back(), s_receiverId);
        /* Remove the message from the vector */
        messages.pop_back();
        if (unlikely(ret != timerId)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to arm the timer in iteration %d in test case '%s'", \
                i, this->GetName());
            std::for_each(
                messages.begin(),
                messages.end(),
                [](TMessage message) {
                    DestroyMessage(message);
                }
            );
            DestroyTimer(timerId);
            return -1;
        }
        /* Crash if disarm fails to not leave the system in a state
         * with wild timers runnings */
        AssertTrue(timerId == DisarmTimer(timerId));
    }

    DestroyTimer(timerId);
    /* Pass the test synchronously */
    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

int TestBasicTiming::RunSubcase2(void) {

    /* Create and destroy a timer */
    TTimerId startTimerId = CreateTimer("Subcase2Timer");
    if (unlikely(startTimerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the reference timer");
        return -1;
    }
    DestroyTimer(startTimerId);

    /* Create another timer and make sure the old ID was not reused */
    TTimerId timerId = CreateTimer("Subcase2Timer");
    if (unlikely(timerId == TIMER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the second timer");
        return -1;
    }
    DestroyTimer(timerId);

    if (timerId == startTimerId) {

        LogPrint(ELogSeverityLevel_Error, "Timer ID 0x%x got immediately reused in test case '%s'", \
            timerId, this->GetName());
        /* Report failure and return 0 to indicate that the test has started successfully */
        TestRunner::ReportTestResult(TestCase::Result::Failure, "Timer ID 0x%x got immediately reused", \
            timerId);
        return 0;
    }

    /* Try allocating timers until the original ID gets reused (note that the test assumes no
     * applications are running in the background that could allocate a timer and "steal" the
     * ID we are looking for */
    for (u32 i = 0; i < MAX_TIMER_COUNT; i++) {

        timerId = CreateTimer("Subcase2Timer");
        if (unlikely(timerId == TIMER_ID_INVALID)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to create timer at i=%d in test case '%s'", \
                i, this->GetName());
            return -1;
        }
        DestroyTimer(timerId);

        if (timerId == startTimerId) {

            LogPrint(ELogSeverityLevel_Info, "Timer ID 0x%x reused after %d allocations", \
                timerId, i + 1);
            TestRunner::ReportTestResult(TestCase::Result::Success, \
                "Timer ID 0x%x reused after %d allocations", timerId, i + 1);
            return 0;
        }
    }

    /* Report failure and return 0 to indicate that the test has started successfully */
    TestRunner::ReportTestResult(TestCase::Result::Failure, \
        "Timer ID 0x%x never got reused", startTimerId);
    return 0;
}

static void ReceiverBody(TMessage message) {

    if (GetMessageId(message) != TEST_MESSAGE_ID) {

        LogPrint(ELogSeverityLevel_Warning, "Worker 0x%x received unexpected message 0x%x from 0x%x", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        DestroyMessage(message);
        return;
    }

    if (s_timerId != TIMER_ID_INVALID) {

        /* Running subcase 0 - timeout message received successfully */
        TestRunner::ReportTestResult(TestCase::Result::Success);
    }

    DestroyMessage(message);
}
