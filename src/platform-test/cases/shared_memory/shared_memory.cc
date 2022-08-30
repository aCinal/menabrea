#include "shared_memory.hh"
#include <framework/test_runner.hh>
#include <menabrea/log.h>
#include <menabrea/workers.h>
#include <menabrea/cores.h>
#include <menabrea/memory.h>
#include <menabrea/messaging.h>
#include <string.h>

static int WorkerInit(void * arg);
static void WorkerLocalInit(int core);
static void WorkerLocalExit(int core);
static void WorkerExit(void);
static void WorkerBody(TMessage message);

#define TEST_MEMORY_CONTENT       "Testing, attention please!"
#define TEST_MEMORY_CONTENT_SIZE  sizeof(TEST_MEMORY_CONTENT)

u32 TestSharedMemory::GetParamsSize(void) {

    return 0;
}

int TestSharedMemory::ParseParams(char * paramsIn, void * paramsOut) {

    (void) paramsIn;
    (void) paramsOut;
    return 0;
}

int TestSharedMemory::StartTest(void * args) {

    SWorkerConfig config = {
        .Name = "MemSharer",
        .InitArg = args,
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .Parallel = false,
        .UserInit = WorkerInit,
        .UserLocalInit = WorkerLocalInit,
        .UserLocalExit = WorkerLocalExit,
        .UserExit = WorkerExit,
        .WorkerBody = WorkerBody
    };

    if (WORKER_ID_INVALID == DeployWorker(&config)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the test worker");
        return -1;
    }

    return 0;
}

void TestSharedMemory::StopTest(void) {

    /* Nothing to do, the worker requests its termination immediately in the init function */
}

static int WorkerInit(void * arg) {

    (void) arg;

    void * sharedPtr = GetMemory(TEST_MEMORY_CONTENT_SIZE);
    if (sharedPtr == nullptr) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate %ld bytes of shared memory", \
            TEST_MEMORY_CONTENT_SIZE);
        return -1;
    }

    /* Copy the data into the shared buffer */
    (void) strcpy(static_cast<char *>(sharedPtr), TEST_MEMORY_CONTENT);

    /* Save the pointer in the worker's context in the platform */
    SetSharedData(sharedPtr);

    /* Request termination, so that exit functions get called immediately */
    TerminateWorker(WORKER_ID_INVALID);

    return 0;
}

static void WorkerLocalInit(int core) {

    int * localData = static_cast<int *>(GetMemory(sizeof(int)));
    if (localData == nullptr) {

        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Failed to allocate local data on core %d", core);
        return;
    }
    *localData = core;
    SetLocalData(localData);

    char * sharedData = static_cast<char *>(GetSharedData());
    if (sharedData == nullptr) {

        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Shared data not available on core %d", core);
        return;
    }

    if (0 != strcmp(sharedData, TEST_MEMORY_CONTENT)) {

        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Unexpected shared data content on core %d: %s", core, sharedData);
    }
}

static void WorkerLocalExit(int core) {

    /* Get data local to the current core */
    int * localData = static_cast<int *>(GetLocalData());
    if (localData == nullptr) {

        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Local data not available on core %d", core);
        return;
    }

    if (*localData != core) {

        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Invalid content of local data (%d) on core %d", \
            *localData, core);
    }

    PutMemory(localData);
    SetLocalData(nullptr);
}

static void WorkerExit(void) {

    void * sharedData = GetSharedData();
    if (sharedData == nullptr) {

        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Shared data null in the exit function");
        return;
    }

    PutMemory(sharedData);
    TestRunner::ReportTestResult(TestCase::Result::Success);
}

static void WorkerBody(TMessage message) {

    LogPrint(ELogSeverityLevel_Error, "Worker 0x%x received unexpected message 0x%x from 0x%x", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
}
