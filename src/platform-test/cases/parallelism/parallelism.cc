#include "parallelism.hh"
#include <framework/test_runner.hh>
#include <framework/params_parser.hh>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/memory.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <vector>
#include <algorithm>

struct TestParallelismParams {
    u32 WorkerCount;
    u32 RoundsPerWorker;
    u32 LoopsPerRound;
    bool UseAtomics;
    bool UseSpinlock;
};

struct TestSharedData {
    /* Counters under test */
    u64 Counter;
    TAtomic64 AtomicCounter;
    /* Counter initialized to the number of workers and decremented each
     * time a worker finishes its job */
    TAtomic64 WorkersRemaining;
    /* Spinlock used if UseSpinlock is set to true */
    TSpinlock Spinlock;
    /* Target value of Counter or AtomicCounter */
    u64 ExpectedValue;
    /* Common config shared between workers */
    u32 LoopsPerRound;
    bool UseAtomics;
    bool UseSpinlock;
};

struct TestWorkerData {
    TestSharedData * SharedData;
    TSpinlock LocalSpinlock;
    u32 RoundsRemaining;
};

static constexpr const TMessageId TRIGGER_MESSAGE_ID = 0x1234;

static int WorkerInit(void * arg);
static void WorkerExit(void);
static void WorkerBody(TMessage message);
static void TouchData(void);
static u64 ReadFinalCounterValue(void);

static std::vector<TWorkerId> s_workers;

u32 TestParallelism::GetParamsSize(void) {

    return sizeof(TestParallelismParams);
}

int TestParallelism::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["workers"] = ParamsParser::StructField(offsetof(TestParallelismParams, WorkerCount), sizeof(u32), ParamsParser::FieldType::U32);
    paramsLayout["rounds"] = ParamsParser::StructField(offsetof(TestParallelismParams, RoundsPerWorker), sizeof(u32), ParamsParser::FieldType::U32);
    paramsLayout["loops"] = ParamsParser::StructField(offsetof(TestParallelismParams, LoopsPerRound), sizeof(u32), ParamsParser::FieldType::U32);
    paramsLayout["useAtomics"] = ParamsParser::StructField(offsetof(TestParallelismParams, UseAtomics), sizeof(bool), ParamsParser::FieldType::Boolean);
    paramsLayout["useSpinlock"] = ParamsParser::StructField(offsetof(TestParallelismParams, UseSpinlock), sizeof(bool), ParamsParser::FieldType::Boolean);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestParallelism::StartTest(void * args) {

    TestParallelismParams * params = static_cast<TestParallelismParams *>(args);

    /* Allocate the context shared between the workers */
    TestSharedData * sharedMem = static_cast<TestSharedData *>(GetMemory(sizeof(TestSharedData)));
    if (sharedMem == nullptr) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate the shared memory for test '%s'", \
            this->GetName());
        return -1;
    }

    /* Initialize the counters under test */
    sharedMem->Counter = 0;
    Atomic64Init(&sharedMem->AtomicCounter);

    /* Initialize the end-of-test condition counter */
    Atomic64Init(&sharedMem->WorkersRemaining);
    Atomic64Set(&sharedMem->WorkersRemaining,  params->WorkerCount);

    /* Initialize the spinlock always though its use depends on the value of
     * the argument 'useSpinlock' */
    SpinlockInit(&sharedMem->Spinlock);

    /* Initialize the target value - if each of N workers does M rounds with K loops in each,
     * then the total number of times the counter gets incremented should be equal to N*M*K */
    sharedMem->ExpectedValue = params->WorkerCount * params->RoundsPerWorker * params->LoopsPerRound;

    /* Initialize config data common to all workers */
    sharedMem->LoopsPerRound = params->LoopsPerRound;
    sharedMem->UseAtomics = params->UseAtomics;
    sharedMem->UseSpinlock = params->UseSpinlock;

    LogPrint(ELogSeverityLevel_Info, "Deploying %d workers that will complete over a shared resource...", \
        params->WorkerCount);
    /* Deploy the workers */
    SWorkerConfig workerConfig = {
        .Name = "ParallelWorker",
        .WorkerId = WORKER_ID_INVALID,
        /* Let the same worker execute on multiple cores at the same time */
        .CoreMask = GetAllCoresMask(),
        .Parallel = true,
        .UserInit = WorkerInit,
        .UserExit = WorkerExit,
        .WorkerBody = WorkerBody
    };
    for (u32 i = 0; i < params->WorkerCount; i++) {

        /* Allocate private data for each worker */
        TestWorkerData * workerPrivateData = static_cast<TestWorkerData *>(GetMemory(sizeof(TestWorkerData)));
        if (unlikely(workerPrivateData == nullptr)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to allocate private data for worker %d in test '%s'", \
                i, this->GetName());
            /* Terminate the workers created thus far */
            std::for_each(
                s_workers.begin(),
                s_workers.end(),
                [](TWorkerId id) { TerminateWorker(id); }
            );
            /* Free the shared memory */
            PutMemory(sharedMem);
            /* Fail the test synchronously */
            return -1;
        }

        /* Initialize per-worker spinlock used to serialize access to
         * the worker's private counters */
        SpinlockInit(&workerPrivateData->LocalSpinlock);
        /* Initialize the number of rounds until end of test */
        workerPrivateData->RoundsRemaining = params->RoundsPerWorker;
        /* Save a reference to the shared memory - the worker will
         * increment the reference count to this memory in its global
         * init function */
        workerPrivateData->SharedData = sharedMem;

        /* Pass the private data via the init function argument */
        workerConfig.InitArg = workerPrivateData;
        TWorkerId workerId = DeployWorker(&workerConfig);
        if (unlikely(workerId == WORKER_ID_INVALID)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to deploy worker %d in test '%s'", \
                i, this->GetName());
            /* Terminate the workers created thus far */
            std::for_each(
                s_workers.begin(),
                s_workers.end(),
                [](TWorkerId id) { TerminateWorker(id); }
            );
            /* Free the shared memory */
            PutMemory(sharedMem);
            /* Free the private data of the worker whose deployment failed */
            PutMemory(workerPrivateData);
            /* Fail the test synchronously */
            return -1;
        }
        s_workers.push_back(workerId);
    }

    /* Put the memory - each worker references it once, so the buffer
     * will be released only when all workers terminate at the end
     * of the test */
    PutMemory(sharedMem);

    std::vector<TMessage> triggerMessages;
    /* Create as many messages as there are workers */
    for (u32 i = 0; i < params->WorkerCount; i++) {

        TMessage message = CreateMessage(TRIGGER_MESSAGE_ID, 0);
        if (unlikely(message == MESSAGE_INVALID)) {

            LogPrint(ELogSeverityLevel_Error, "Failed to create a trigger message %d in test '%s'", \
                i, this->GetName());
            /* Destroy the messages created thus far */
            std::for_each(
                triggerMessages.begin(),
                triggerMessages.end(),
                [](TMessage message) { DestroyMessage(message); }
            );
            /* Terminate all the workers */
            std::for_each(
                s_workers.begin(),
                s_workers.end(),
                [](TWorkerId id) { TerminateWorker(id); }
            );
            /* Fail the test synchronously */
            return -1;
        }

        triggerMessages.push_back(message);
    }

    /* Send the trigger messages */
    for (u32 i = 0; i < params->WorkerCount; i++) {

        SendMessage(triggerMessages[i], s_workers[i]);
    }

    /* Let the triggerMessages vector go out of scope - the handles will get
     * cleaned up, but the underlying messages have already been sent */

    return 0;
}

void TestParallelism::StopTest(void) {

    /* Terminate each worker - private memory will be freed by each worker's
     * global exit and each worker will also decrement the reference count of
     * the shared data which will be freed when the count reaches zero */
    std::for_each(
        s_workers.begin(),
        s_workers.end(),
        [](TWorkerId id) { TerminateWorker(id); }
    );
    s_workers.clear();
}

static int WorkerInit(void * arg) {

    TestWorkerData * workerData = static_cast<TestWorkerData *>(arg);
    /* Reference the shared memory, so that its lifetime is at least
     * the lifetime of the current worker */
    RefMemory(workerData->SharedData);
    SetSharedData(workerData);

    return 0;
}

static void WorkerExit(void) {

    TestWorkerData * workerData = static_cast<TestWorkerData *>(GetSharedData());
    /* Decrement reference counter of the shared memory */
    PutMemory(workerData->SharedData);
    /* Free private memory */
    PutMemory(workerData);
}

static void WorkerBody(TMessage message) {

    AssertTrue(GetMessageId(message) == TRIGGER_MESSAGE_ID);

    TestWorkerData * data = static_cast<TestWorkerData *>(GetSharedData());

    /* Access the memory according to the test conditions */
    for (u32 i = 0; i < data->SharedData->LoopsPerRound; i++) {

        TouchData();
    }

    SpinlockAcquire(&data->LocalSpinlock);
    if (data->RoundsRemaining > 1) {

        data->RoundsRemaining--;
        /* Resend the message back to self */
        SendMessage(message, GetOwnWorkerId());

    } else if (data->RoundsRemaining == 1) {

        /* Last round - end of the test for the current worker - our work here is done */
        DestroyMessage(message);

        if (Atomic64SubReturn(&data->SharedData->WorkersRemaining, 1) == 0) {

            /* All other workers have terminated, check the result */
            u64 counterValue = ReadFinalCounterValue();
            if (counterValue == data->SharedData->ExpectedValue) {

                TestRunner::ReportTestResult(TestCase::Result::Success);

            } else {

                LogPrint(ELogSeverityLevel_Warning, "Incorrect counter value: %ld, expected: %ld", \
                    counterValue, data->SharedData->ExpectedValue);
                TestRunner::ReportTestResult(TestCase::Result::Failure, \
                    "Incorrect counter value: %ld, expected: %ld", \
                    counterValue, data->SharedData->ExpectedValue);
            }
        }
        TerminateWorker(WORKER_ID_INVALID);
        /* Note that TerminateWorker returns and the worker continues to release the spinlock
         * - no deadlock may occur */

    } else {

        /* RoundRemaining == 0, destroy the message and quit (termination has been or will be
         * requested in parallel on a different core) */
        DestroyMessage(message);
    }

    SpinlockRelease(&data->LocalSpinlock);
}

static void TouchData(void){

    TestWorkerData * data = static_cast<TestWorkerData *>(GetSharedData());

    /* Acquire the spinlock if in use */
    if (data->SharedData->UseSpinlock) {
        SpinlockAcquire(&data->SharedData->Spinlock);
    }

    /* Do the actual memory access */
    if (data->SharedData->UseAtomics) {
        Atomic64Inc(&data->SharedData->AtomicCounter);
    } else {
        data->SharedData->Counter++;
    }

    /* Release the spinlock if previously acquired */
    if (data->SharedData->UseSpinlock) {
        SpinlockRelease(&data->SharedData->Spinlock);
    }
}

static u64 ReadFinalCounterValue(void) {

    TestWorkerData * data = static_cast<TestWorkerData *>(GetSharedData());
    return data->SharedData->UseAtomics ? Atomic64Get(&data->SharedData->AtomicCounter) : data->SharedData->Counter;
}
