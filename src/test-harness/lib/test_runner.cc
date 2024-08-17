#define FORMAT_LOG(fmt)                        "test_runner.cc: " fmt
#include <logging.hh>
#include <test_runner.hh>
#include <ipc_socket.hh>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/timing.h>
#include <menabrea/memory.h>
#include <menabrea/cores.h>
#include <menabrea/exception.h>
#include <stdarg.h>
#include <alloca.h>

namespace TestRunner {

#define REPORT_GOOD(fmt, ...)                  IpcSocket::WriteBack("[+] " fmt, ##__VA_ARGS__)
#define REPORT_BAD(fmt, ...)                   IpcSocket::WriteBack("[-] " fmt, ##__VA_ARGS__)
#define REPORT_TEST_CASE_SUCCESS(name, extra) { \
    int __cmp = strcmp((extra), " "); \
    REPORT_GOOD("Test case '%s' -- PASSED%s%s%s", \
        (name), \
        __cmp ? " (" : "", \
        __cmp ? (extra) : "", \
        __cmp ? ")" : ""); \
}
#define REPORT_TEST_CASE_FAILURE(name, extra)  REPORT_BAD("Test case '%s' -- FAILED (%s)", (name), (extra))

enum class State {
    Idle,
    Busy
};

enum class Command {
    Run,
    Stop,
    Invalid
};

static Command ParseCommandString(const char * commandString);
static void HandleCommandRun(const char * testCaseName, char * testArgsString);
static void HandleCommandStop(void);
static void ListenerBody(TMessage message);
static void HandleTestResult(TMessage message);
static void HandleTimeoutExtension(TMessage message);
static TMessage CreateTimeoutMessage(void);
static void StopOngoingTest(void);
static u64 CurrentTestRunId(void);
static void StartNewTestRun(void);

static constexpr const TMessageId TEST_RESULT_MSG_ID = 0x1410;
static constexpr const u32 TEST_RESULT_EXTRA_LEN = 160;
struct STestResult {
    u64 TestRunId;
    TestCase::Result Result;
    int Core;
    char Extra[TEST_RESULT_EXTRA_LEN];
};
static constexpr const TMessageId TEST_TIMEOUT_EXTENSION_MSG_ID = 0x1411;
struct STestTimeoutExtension {
    u64 RemainingTime;
    u64 TestRunId;
    int Core;
};

static TWorkerId s_listenerId = WORKER_ID_INVALID;
static State s_currentState = State::Idle;
static TTimerId s_timeoutTimer = TIMER_ID_INVALID;
static TestCase::Instance * s_currentTestCase = nullptr;
static TAtomic64 * s_testRunCounterPtr = nullptr;

void Init(void) {

    s_testRunCounterPtr = static_cast<TAtomic64 *>(GetInitMemory(sizeof(TAtomic64)));
    AssertTrue(s_testRunCounterPtr != nullptr);
    Atomic64Init(s_testRunCounterPtr);

    s_listenerId = DeploySimpleWorker("TestResultListener", WORKER_ID_INVALID, GetSharedCoreMask(), ListenerBody);
    AssertTrue(s_listenerId != WORKER_ID_INVALID);
}

void Teardown(void) {

    if (s_currentState == State::Busy) {

        StopOngoingTest();
    }
    TerminateWorker(s_listenerId);
}

void OnPollIn(char * inputString) {

    /* Tokenize the input string */
    char * saveptr = nullptr;
    char * commandString = strtok_r(inputString, " ", &saveptr);
    char * testCaseName = strtok_r(nullptr, " ", &saveptr);
    /* Save the rest of the string as test parameters */
    /* TODO: Consider portability. strtok_r manpage is vague about how
     * saveptr is used. It may not need to be the pointer to the remainder
     * on all implementations. */
    char * testArgsString = saveptr;

    /* Parse the runner command */
    Command command = ParseCommandString(commandString);
    switch (command) {
    case Command::Run:
        if (testCaseName != nullptr) {

            HandleCommandRun(testCaseName, testArgsString);

        } else {

            LOG_WARNING("Missing test case name!");
            REPORT_BAD("Missing test case name!");
        }
        break;

    case Command::Stop:
        HandleCommandStop();
        break;

    default:
        LOG_WARNING("Invalid command: '%s'", commandString);
        REPORT_BAD("Invalid command: '%s'", commandString);
        break;
    }
}

void OnPollHup(void) {

    if (s_currentState == State::Busy) {

        StopOngoingTest();
    }
}

void ReportTestResult(TestCase::Result result, const char * extra, va_list args) {

    AssertTrue(extra != nullptr);

    u64 currentTestRun = CurrentTestRunId();
    /* Create a result message */
    TMessage resultMessage = CreateMessage(TEST_RESULT_MSG_ID, sizeof(STestResult));
    /* This API is only called from the test code at a point when the test case timeout
     * timer is already ticking, and so we allow the allocation of the result report to
     * fail - the test will time out normally. */
    if (unlikely(resultMessage == MESSAGE_INVALID)) {

        LOG_ERROR("Failed to create the test result report for test run %ld", currentTestRun);
        return;
    }

    /* Fill in the payload */
    STestResult * payload = static_cast<STestResult *>(GetMessagePayload(resultMessage));
    payload->TestRunId = currentTestRun;
    payload->Result = result;
    payload->Core = GetCurrentCore();
    (void) vsnprintf(payload->Extra, sizeof(payload->Extra), extra, args);

    /* Send the message to the listener */
    SendMessage(resultMessage, s_listenerId);
}

void ExtendTimeout(u64 remainingTime) {

    u64 currentTestRun = CurrentTestRunId();
    /* Create a request message */
    TMessage extensionRequest = CreateMessage(TEST_TIMEOUT_EXTENSION_MSG_ID, sizeof(STestTimeoutExtension));
    /* Allow the allocation of the timeout extension request to fail - the test will time out normally */
    if (unlikely(extensionRequest == MESSAGE_INVALID)) {

        LOG_ERROR("Failed to create the timeout extension request for test run %ld", currentTestRun);
        return;
    }

    /* Fill in the payload */
    STestTimeoutExtension * payload = static_cast<STestTimeoutExtension *>(GetMessagePayload(extensionRequest));
    payload->TestRunId = currentTestRun;
    payload->Core = GetCurrentCore();
    payload->RemainingTime = remainingTime;

    /* Send the request to the listener */
    SendMessage(extensionRequest, s_listenerId);
}

static Command ParseCommandString(const char * commandString) {

    if (0 == strcmp(commandString, "run")) {

        return Command::Run;

    } else if (0 == strcmp(commandString, "stop")) {

        return Command::Stop;

    } else {

        return Command::Invalid;
    }
}

static void HandleCommandRun(const char * testCaseName, char * testArgsString) {

    if (s_currentState != State::Idle) {

        AssertTrue(s_currentTestCase != nullptr);
        LOG_WARNING("Cannot run '%s' - runner in state %d running test '%s'", \
            testCaseName, static_cast<int>(s_currentState), s_currentTestCase->GetName());
        REPORT_BAD("Runner is busy running test case '%s'", \
            s_currentTestCase->GetName());
        return;
    }

    /* Move on to the next test run ID */
    StartNewTestRun();

    /* Look up the test case instance */
    TestCase::Instance * testCaseInstance = TestCase::GetInstanceByName(testCaseName);
    if (testCaseInstance == nullptr) {

        LOG_WARNING("Test case '%s' not registered", testCaseName);
        REPORT_BAD("Test case '%s' not registered", testCaseName);
        return;
    }

    /* Allocate memory for the test case parameters if needed */
    u32 paramsSize = testCaseInstance->GetParamsSize();
    void * testParams = nullptr;
    if (paramsSize > 0) {

        /* Check that some test case parameters were actually provided */
        if (testArgsString == nullptr) {

            LOG_WARNING("Missing parameters for case '%s'", testCaseName);
            REPORT_BAD("Missing parameters for case '%s'", testCaseName);
            return;
        }

        /* Allocate memory for the parameters */
        testParams = alloca(paramsSize);
        if (unlikely(testParams == nullptr)) {

            LOG_ERROR("%s(): Failed to allocate %d bytes for the parameters of test case '%s'", \
                __FUNCTION__, paramsSize, testCaseName);
            REPORT_BAD("Failed to allocate memory for the parameters of test case '%s'", \
                testCaseName);
            return;
        }

        /* Parse the test case parameters */
        if (unlikely(testCaseInstance->ParseParams(testArgsString, testParams))) {

            LOG_ERROR("%s(): Failed to parse parameters for test case '%s'", \
                __FUNCTION__, testCaseName);
            REPORT_BAD("Failed to parse the test case parameteres for test '%s'", \
                testCaseName);
            return;
        }
    }

    /* Create a timeout timer */
    char timerName[64];
    (void) snprintf(timerName, sizeof(timerName), "tmo_%s", testCaseName);
    /* Create a timeout timer */
    TTimerId timeoutTimer = CreateTimer(timerName);
    if (unlikely(timeoutTimer == TIMER_ID_INVALID)) {

        LOG_ERROR("%s(): Failed to create a timeout timer for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to allocate necessary resources to run test case '%s'", \
            testCaseName);
        return;
    }

    /* Create a timeout message */
    TMessage timeoutMessage = CreateTimeoutMessage();
    if (unlikely(timeoutMessage == MESSAGE_INVALID)) {

        LOG_ERROR("%s(): Failed to create a timeout message for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to allocate necessary resources to run test case '%s'", \
            testCaseName);
        DestroyTimer(timeoutTimer);
        return;
    }

    AssertTrue(s_listenerId != WORKER_ID_INVALID);
    /* Arm the timer */
    if (unlikely(timeoutTimer != ArmTimer(timeoutTimer, TestCase::DEFAULT_TEST_TIMEOUT, 0, timeoutMessage, s_listenerId))) {

        LOG_ERROR("%s(): Failed to arm the timeout timer for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to allocate necessary resources to run test case '%s'", \
            testCaseName);
        DestroyTimer(timeoutTimer);
        DestroyMessage(timeoutMessage);
        return;
    }

    LOG_INFO("Starting test case '%s' in test run %ld...", \
        testCaseInstance->GetName(), CurrentTestRunId());
    /* Call virtual start method */
    if (unlikely(testCaseInstance->StartTest(testParams))) {

        LOG_ERROR("%s(): Virtual start function failed for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to start test case '%s'", testCaseName);
        AssertTrue(timeoutTimer == DisarmTimer(timeoutTimer));
        DestroyTimer(timeoutTimer);
        return;
    }

    /* Save the handles globally */
    s_timeoutTimer = timeoutTimer;
    s_currentTestCase = testCaseInstance;
    /* Do a state transition */
    s_currentState = State::Busy;
}

static void HandleCommandStop(void) {

    if (s_currentState != State::Busy) {

        REPORT_BAD("No test case currently running");
        return;
    }

    StopOngoingTest();
}

static void ListenerBody(TMessage message) {

    switch (GetMessageId(message)) {
    case TEST_RESULT_MSG_ID:
        HandleTestResult(message);
        break;

    case TEST_TIMEOUT_EXTENSION_MSG_ID:
        HandleTimeoutExtension(message);
        break;

    default:
        LOG_WARNING("Worker 0x%x received unexpected message 0x%x from 0x%x", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        break;
    }

    DestroyMessage(message);
}

static void HandleTestResult(TMessage message) {

    STestResult * payload = static_cast<STestResult *>(GetMessagePayload(message));
    /* Check if result from the current test */
    if (s_currentState != State::Busy or payload->TestRunId != CurrentTestRunId()) {

        LOG_WARNING( \
            "Received obsolete test result report from 0x%x on core %d (testRunId=%ld, currentTestRunId=%ld, runnerState=%d, result=%s, extra='%s')", \
            GetMessageSender(message), payload->Core, payload->TestRunId, CurrentTestRunId(), static_cast<int>(s_currentState), \
            payload->Result == TestCase::Result::Success ? "success" : "failure", payload->Extra);
        return;
    }
    /* We are in busy state, a test case must be ongoing */
    AssertTrue(s_currentTestCase != nullptr);

    /* Report result via UDS */
    if (payload->Result == TestCase::Result::Success) {
        REPORT_TEST_CASE_SUCCESS(s_currentTestCase->GetName(), payload->Extra);
    } else {
        REPORT_TEST_CASE_FAILURE(s_currentTestCase->GetName(), payload->Extra);
    }

    /* Stop the ongoing test case */
    StopOngoingTest();
}

static void HandleTimeoutExtension(TMessage message) {

    STestTimeoutExtension * payload = static_cast<STestTimeoutExtension *>(GetMessagePayload(message));
    /* Check if result from the current test */
    if (s_currentState != State::Busy or payload->TestRunId != CurrentTestRunId()) {

        LOG_WARNING( \
            "Received obsolete test timeout extension request from 0x%x on core %d (testRunId=%ld, currentTestRunId=%ld, runnerState=%d)", \
            GetMessageSender(message), payload->Core, payload->TestRunId, CurrentTestRunId(), static_cast<int>(s_currentState));
        return;
    }
    /* We are in busy state, a test case must be ongoing */
    AssertTrue(s_currentTestCase != nullptr);

    /* Disarm and rearm the timer with the new timeout message */
    TMessage timeoutMessage = CreateTimeoutMessage();
    if (unlikely(timeoutMessage == MESSAGE_INVALID)) {

        LOG_ERROR("Failed to create a new timeout message to satisfy the timeout extension request");
        return;
    }

    AssertTrue(s_timeoutTimer != TIMER_ID_INVALID);
    /* Stop the current timeout */
    AssertTrue(s_timeoutTimer == DisarmTimer(s_timeoutTimer));
    /* Rearm the timer */
    if (unlikely(s_timeoutTimer != ArmTimer(s_timeoutTimer, payload->RemainingTime, 0, timeoutMessage, s_listenerId))) {

        LOG_ERROR("%s(): Failed to rearm the timeout timer for test case '%s'", \
            __FUNCTION__, s_currentTestCase->GetName());
        REPORT_BAD("Failed to extend timeout for test case '%s'", s_currentTestCase->GetName());
        DestroyMessage(timeoutMessage);
        /* Stop the test immediately */
        StopOngoingTest();
        return;
    }

    LOG_INFO("Successfully extended timeout of test '%s' - expires in %ld us", \
        s_currentTestCase->GetName(), payload->RemainingTime);
}

static TMessage CreateTimeoutMessage(void) {

    TMessage message = CreateMessage(TEST_RESULT_MSG_ID, sizeof(STestResult));

    if (likely(message != MESSAGE_INVALID)) {

        /* Fill in the payload */
        STestResult * payload = static_cast<STestResult *>(GetMessagePayload(message));
        payload->Result = TestCase::Result::Failure;
        (void) strncpy(payload->Extra, "Test execution timed out", sizeof(payload->Extra));
        payload->Extra[sizeof(payload->Extra) - 1] = '\0';
        payload->TestRunId = CurrentTestRunId();
        payload->Core = GetCurrentCore();
    }

    return message;
}

static void StopOngoingTest(void) {

    AssertTrue(s_currentTestCase != nullptr);
    LOG_INFO("Stopping test case '%s'...", s_currentTestCase->GetName());
    s_currentTestCase->StopTest();
    AssertTrue(s_timeoutTimer == DisarmTimer(s_timeoutTimer));
    DestroyTimer(s_timeoutTimer);
    s_timeoutTimer = TIMER_ID_INVALID;
    s_currentTestCase = nullptr;
    s_currentState = State::Idle;
}

static u64 CurrentTestRunId(void) {

    AssertTrue(s_testRunCounterPtr != nullptr);
    /* Atomically load the current test run ID from shared memory */
    return Atomic64Get(s_testRunCounterPtr);
}

static void StartNewTestRun(void) {

    AssertTrue(s_testRunCounterPtr != nullptr);
    /* Atomically increment the current test run ID in shared memory */
    Atomic64Inc(s_testRunCounterPtr);
}

}
