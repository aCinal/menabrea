#include "basic_workers.hh"
#include <framework/test_runner.hh>
#include <framework/params_parser.hh>
#include <menabrea/workers.h>
#include <menabrea/cores.h>
#include <menabrea/messaging.h>
#include <menabrea/log.h>

struct TestBasicWorkersParams {
    u32 Subcase;
};

static constexpr const TWorkerId DUMMY_WORKER_ID = 0x420;

static int Subcase2GlobalInit(void * arg);
static void Subcase3LocalInit(int core);
static int Subcase9GlobalInit(void * arg);
static void DummyBody(TMessage message);

u32 TestBasicWorkers::GetParamsSize(void) {

    return sizeof(TestBasicWorkersParams);
}

int TestBasicWorkers::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["subcase"] = ParamsParser::StructField(offsetof(TestBasicWorkersParams, Subcase), sizeof(u32), ParamsParser::FieldType::U32);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestBasicWorkers::StartTest(void * args) {

    TestBasicWorkersParams * params = static_cast<TestBasicWorkersParams *>(args);

    switch (params->Subcase) {
    case 0:
        return RunSubcase0();

    case 1:
        return RunSubcase1();

    case 2:
        return RunSubcase2();

    case 3:
        return RunSubcase3();

    case 4:
        return RunSubcase4();

    case 5:
        return RunSubcase5();

    case 6:
        return RunSubcase6();

    case 7:
        return RunSubcase7();

    case 8:
        return RunSubcase8();

    case 9:
        return RunSubcase9();

    default:
        LogPrint(ELogSeverityLevel_Error, "Invalid subcase %d for test case '%s'", \
            params->Subcase, this->GetName());
        return -1;
    }
}

void TestBasicWorkers::StopTest(void) {

    /* Nothing to do */
}

int TestBasicWorkers::RunSubcase0(void) {

    /* Deploy worker and terminate immediately */
    TWorkerId workerId = DeploySimpleWorker("Subcase0Worker", WORKER_ID_INVALID, GetAllCoresMask(), DummyBody);
    if (unlikely(WORKER_ID_INVALID == workerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy worker in subcase 0 of test '%s'", \
            this->GetName());
        return -1;
    }

    TerminateWorker(workerId);
    TestRunner::ReportTestResult(TestCase::Result::Success, " ");
    return 0;
}

int TestBasicWorkers::RunSubcase1(void) {

    /* Try terminating twice - the platform should handle this gracefully
     * and not raise an exception */

    TWorkerId workerId = DeploySimpleWorker("Subcase1Worker", WORKER_ID_INVALID, GetAllCoresMask(), DummyBody);
    if (unlikely(WORKER_ID_INVALID == workerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy worker in subcase 1 of test '%s'", \
            this->GetName());
        return -1;
    }

    LogPrint(ELogSeverityLevel_Info, "%s: Terminating worker 0x%x twice...", \
        this->GetName(), workerId);
    TerminateWorker(workerId);
    TerminateWorker(workerId);

    TestRunner::ReportTestResult(TestCase::Result::Success, " ");
    return 0;
}

int TestBasicWorkers::RunSubcase2(void) {

    SWorkerConfig workerConfig = {
        .Name = "Subcase2Worker",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .UserInit = Subcase2GlobalInit,
        .WorkerBody = DummyBody
    };
    /* Terminate synchronously in the global init */
    if (unlikely(WORKER_ID_INVALID == DeployWorker(&workerConfig))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy worker in subcase 2 of test '%s'", \
            this->GetName());
        return -1;
    }

    TestRunner::ReportTestResult(TestCase::Result::Success, " ");
    return 0;
}

int TestBasicWorkers::RunSubcase3(void) {

    SWorkerConfig workerConfig = {
        .Name = "Subcase3Worker",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .UserLocalInit = Subcase3LocalInit,
        .WorkerBody = DummyBody
    };
    /* Terminate asynchronously in the local init */
    if (unlikely(WORKER_ID_INVALID == DeployWorker(&workerConfig))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy worker in subcase 3 of test '%s'", \
            this->GetName());
        return -1;
    }

    /* Test started successfully, result will be reported asynchronously */
    return 0;
}

int TestBasicWorkers::RunSubcase4(void) {

    /* Try deploying two workers with the same static ID */
    TWorkerId workerId1 = DeploySimpleWorker("Subcase4Worker", DUMMY_WORKER_ID, GetAllCoresMask(), DummyBody);
    if (unlikely(WORKER_ID_INVALID == workerId1)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy worker in subcase 4 of test '%s'", \
            this->GetName());
        return -1;
    }

    LogPrint(ELogSeverityLevel_Info, "Trying to register worker with local ID 0x%x a second time...", DUMMY_WORKER_ID);
    TWorkerId workerId2 = DeploySimpleWorker("Subcase4Worker", DUMMY_WORKER_ID, GetAllCoresMask(), DummyBody);
    if (unlikely(WORKER_ID_INVALID != workerId2)) {

        TerminateWorker(workerId1);
        TerminateWorker(workerId2);
        LogPrint(ELogSeverityLevel_Error, "Second deployment of worker 0x%x unexpectedly succeeded (DeployWorker returned 0x%x) in test '%s'", \
            DUMMY_WORKER_ID, workerId2, this->GetName());
        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Second deployment of worker 0x%x unexpectedly succeeded", \
            DUMMY_WORKER_ID);
        return 0;
    }

    TerminateWorker(workerId1);
    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

int TestBasicWorkers::RunSubcase5(void) {

    /* Try deploying a worker with static ID in the dynamic range */
    TWorkerId workerId = DeploySimpleWorker("Subcase5Worker", WORKER_ID_DYNAMIC_BASE, GetAllCoresMask(), DummyBody);
    if (unlikely(workerId != WORKER_ID_INVALID)) {

        TerminateWorker(workerId);
        LogPrint(ELogSeverityLevel_Error, \
            "Unexpectedly succeeded at deploying a worker with static ID 0x%x in the dynamic range during test '%s'", \
            WORKER_ID_DYNAMIC_BASE, this->GetName());
        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Unexpectedly succeeded at deploying a worker with static ID 0x%x", \
            DUMMY_WORKER_ID);
        return 0;
    }

    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

int TestBasicWorkers::RunSubcase6(void) {

    /* Pass NULL pointer for worker config */
    TWorkerId workerId = DeployWorker(nullptr);
    if (unlikely(workerId != WORKER_ID_INVALID)) {

        TerminateWorker(workerId);
        LogPrint(ELogSeverityLevel_Error, \
            "Unexpectedly succeeded at deploying worker 0x%x with NULL pointer for config during test '%s'", \
            workerId, this->GetName());
        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Unexpectedly succeeded at deploying worker 0x%x with NULL pointer for config", \
            workerId);
        return 0;
    }

    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

int TestBasicWorkers::RunSubcase7(void) {

    /* Pass NULL pointer for worker name */
    SWorkerConfig workerConfig = {
        .Name = nullptr,
        .WorkerBody = DummyBody
    };
    TWorkerId workerId = DeployWorker(&workerConfig);
    if (unlikely(workerId != WORKER_ID_INVALID)) {

        TerminateWorker(workerId);
        LogPrint(ELogSeverityLevel_Error, \
            "Unexpectedly succeeded at deploying worker 0x%x with NULL pointer for name during test '%s'", \
            workerId, this->GetName());
        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Unexpectedly succeeded at deploying worker 0x%x with NULL pointer for name", \
            workerId);
        return 0;
    }

    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

int TestBasicWorkers::RunSubcase8(void) {

    /* Pass NULL pointer for worker body */
    SWorkerConfig workerConfig = {
        .Name = "Subcase8Worker",
        .WorkerBody = nullptr
    };
    TWorkerId workerId = DeployWorker(&workerConfig);
    if (unlikely(workerId != WORKER_ID_INVALID)) {

        TerminateWorker(workerId);
        LogPrint(ELogSeverityLevel_Error, \
            "Unexpectedly succeeded at deploying worker 0x%x with NULL pointer for worker body during test '%s'", \
            workerId, this->GetName());
        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Unexpectedly succeeded at deploying worker 0x%x with NULL pointer for worker body", \
            workerId);
        return 0;
    }

    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

int TestBasicWorkers::RunSubcase9(void) {

    SWorkerConfig workerConfig = {
        .Name = "Subcase9Worker",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .UserInit = Subcase9GlobalInit,
        .WorkerBody = DummyBody
    };
    /* Encounter a failure in application callback (user init) */
    TWorkerId workerId = DeployWorker(&workerConfig);
    if (unlikely(workerId != WORKER_ID_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, \
            "Unexpectedly succeeded at deploying worker 0x%x despite user init failure during test '%s'", \
            workerId, this->GetName());
        TestRunner::ReportTestResult(TestCase::Result::Failure, \
            "Unexpectedly succeeded at deploying worker 0x%x despite user init failure", \
            workerId);
        return -1;
    }

    TestRunner::ReportTestResult(TestCase::Result::Success);
    return 0;
}

static int Subcase2GlobalInit(void * arg) {

    (void) arg;
    /* Terminate current worker */
    TerminateWorker(WORKER_ID_INVALID);
    return 0;
}

static void Subcase3LocalInit(int core) {

    /* Terminate only on a single core - multiple terminations
     * are tested separately */
    if (core == 0) {

        /* Terminate current worker */
        TerminateWorker(WORKER_ID_INVALID);
        TestRunner::ReportTestResult(TestCase::Result::Success);
    }
}

static int Subcase9GlobalInit(void * arg) {

    (void) arg;
    /* Fail the user init */
    return -1;
}

static void DummyBody(TMessage message) {

    DestroyMessage(message);
}

