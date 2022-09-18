#define FORMAT_LOG(fmt)                        "test_runner.cc: " fmt
#include <framework/logging.hh>
#include <framework/test_runner.hh>
#include <framework/ipc_socket.hh>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/timing.h>
#include <menabrea/memory.h>
#include <menabrea/cores.h>
#include <menabrea/exception.h>
#include <stdarg.h>

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
static void StopOngoingTest(void);
static u32 CurrentTestRunId(void);
static void StartNewTestRun(void);

static constexpr const u64 TEST_TIMEOUT = 5 * 1000 * 1000;  /* 5 seconds */

static constexpr const TMessageId TEST_RESULT_MSG_ID = 0x1410;
static constexpr const u32 TEST_RESULT_EXTRA_LEN = 160;
struct STestResult {
    u32 TestRunId;
    TestCase::Result Result;
    int Core;
    char Extra[TEST_RESULT_EXTRA_LEN];
};

static TWorkerId s_listenerId = WORKER_ID_INVALID;
static State s_currentState = State::Idle;
static TTimerId s_timeoutTimer = TIMER_ID_INVALID;
static TestCase::Instance * s_currentTestCase = nullptr;
static env_atomic64_t * s_testRunCounterPtr = nullptr;

void Init(void) {

    s_listenerId = DeploySimpleWorker("TestResultListener", WORKER_ID_INVALID, GetSharedCoreMask(), ListenerBody);
    AssertTrue(s_listenerId != WORKER_ID_INVALID);

    s_testRunCounterPtr = static_cast<env_atomic64_t *>(GetMemory(sizeof(env_atomic64_t)));
    AssertTrue(s_testRunCounterPtr != nullptr);
    env_atomic64_init(s_testRunCounterPtr);
}

void Teardown(void) {

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

void ReportTestResult(TestCase::Result result) {

    ReportTestResult(result, " ");
}

void ReportTestResult(TestCase::Result result, const char * extra, ...) {

    AssertTrue(extra != nullptr);
    /* Create a timeout message */
    TMessage timeoutMessage = CreateMessage(TEST_RESULT_MSG_ID, sizeof(STestResult));
    /* Don't leave the system in an inconsistent state - crash immediately if report
     * allocation fails */
    AssertTrue(timeoutMessage != MESSAGE_INVALID);
    /* Fill in the payload */
    STestResult * payload = static_cast<STestResult *>(GetMessagePayload(timeoutMessage));
    payload->TestRunId = CurrentTestRunId();
    payload->Result = result;
    payload->Core = GetCurrentCore();
    va_list args;
    va_start(args, extra);
    (void) vsnprintf(payload->Extra, sizeof(payload->Extra), extra, args);
    va_end(args);

    /* Send the message to the listener */
    SendMessage(timeoutMessage, s_listenerId);
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
        testParams = GetMemory(paramsSize);
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
            if (testParams) {
                PutMemory(testParams);
            }
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
        if (testParams) {
            PutMemory(testParams);
        }
        return;
    }

    /* Create a timeout message */
    TMessage timeoutMessage = CreateMessage(TEST_RESULT_MSG_ID, sizeof(STestResult));
    if (unlikely(timeoutMessage == MESSAGE_INVALID)) {

        LOG_ERROR("%s(): Failed to create a timeout message for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to allocate necessary resources to run test case '%s'", \
            testCaseName);
        DestroyTimer(timeoutTimer);
        if (testParams) {
            PutMemory(testParams);
        }
        return;
    }
    /* Fill in the payload */
    STestResult * payload = static_cast<STestResult *>(GetMessagePayload(timeoutMessage));
    payload->Result = TestCase::Result::Failure;
    (void) strncpy(payload->Extra, "Test execution timed out", sizeof(payload->Extra));
    payload->Extra[sizeof(payload->Extra) - 1] = '\0';
    payload->TestRunId = CurrentTestRunId();
    payload->Core = GetCurrentCore();

    AssertTrue(s_listenerId != WORKER_ID_INVALID);
    /* Arm the timer */
    if (unlikely(timeoutTimer != ArmTimer(timeoutTimer, TEST_TIMEOUT, 0, timeoutMessage, s_listenerId))) {

        LOG_ERROR("%s(): Failed to arm the timeout timer for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to allocate necessary resources to run test case '%s'", \
            testCaseName);
        DestroyTimer(timeoutTimer);
        DestroyMessage(timeoutMessage);
        if (testParams) {
            PutMemory(testParams);
        }
        return;
    }

    LOG_INFO("Starting test case '%s' in test run %d...", \
        testCaseInstance->GetName(), CurrentTestRunId());
    /* Call virtual start method */
    if (unlikely(testCaseInstance->StartTest(testParams))) {

        LOG_ERROR("%s(): Virtual start function failed for test case '%s'", \
            __FUNCTION__, testCaseName);
        REPORT_BAD("Failed to start test case '%s'", testCaseName);
        AssertTrue(timeoutTimer == DisarmTimer(timeoutTimer));
        DestroyTimer(timeoutTimer);
        if (testParams) {
            PutMemory(testParams);
        }
        return;
    }

    /* Save the handles globally */
    s_timeoutTimer = timeoutTimer;
    s_currentTestCase = testCaseInstance;
    /* Do a state transition */
    s_currentState = State::Busy;
    if (testParams) {
        PutMemory(testParams);
    }
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
            "Received obsolete test result report from 0x%x on core %d (testRunId=%d, currentTestRunId=%d, runnerState=%d, result=%s, extra='%s')", \
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

static void StopOngoingTest(void) {

    AssertTrue(s_currentTestCase != nullptr);
    LogPrint(ELogSeverityLevel_Info, "Stopping test case '%s'...", s_currentTestCase->GetName());
    s_currentTestCase->StopTest();
    AssertTrue(s_timeoutTimer == DisarmTimer(s_timeoutTimer));
    DestroyTimer(s_timeoutTimer);
    s_timeoutTimer = TIMER_ID_INVALID;
    s_currentTestCase = nullptr;
    s_currentState = State::Idle;
}

static u32 CurrentTestRunId(void) {

    AssertTrue(s_testRunCounterPtr != nullptr);
    /* Atomically load the current test run ID from shared memory */
    return env_atomic64_get(s_testRunCounterPtr);
}

static void StartNewTestRun(void) {

    AssertTrue(s_testRunCounterPtr != nullptr);
    /* Atomically increment the current test run ID in shared memory */
    env_atomic64_inc(s_testRunCounterPtr);
}

}
