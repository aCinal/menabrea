#include "parallelism.hh"
#include <menabrea/test/params_parser.hh>
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
    bool UseParallelWorkers;
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
    TSpinlock PrivateSpinlock;
    TAtomic64 OngoingAccesses;
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
    paramsLayout["useParallelWorkers"] = ParamsParser::StructField(offsetof(TestParallelismParams, UseParallelWorkers), sizeof(bool), ParamsParser::FieldType::Boolean);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestParallelism::StartTest(void * args) {

    TestParallelismParams * params = static_cast<TestParallelismParams *>(args);

    /* Allocate the context shared between the workers */
    TestSharedData * sharedMem = \
        static_cast<TestSharedData *>(GetRuntimeMemory(sizeof(TestSharedData)));
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

    LogPrint(ELogSeverityLevel_Info, "Deploying %d worker(s) that will compete over a shared resource...", \
        params->WorkerCount);
    /* Deploy the workers */
    SWorkerConfig workerConfig = {
        .Name = "ParallelismTester",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .Parallel = params->UseParallelWorkers,
        .UserInit = WorkerInit,
        .UserExit = WorkerExit,
        .WorkerBody = WorkerBody
    };
    for (u32 i = 0; i < params->WorkerCount; i++) {

        /* Allocate private data for each worker */
        TestWorkerData * workerPrivateData = \
            static_cast<TestWorkerData *>(GetRuntimeMemory(sizeof(TestWorkerData)));
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
            PutRuntimeMemory(sharedMem);
            /* Fail the test synchronously */
            return -1;
        }

        /* Initialize per-worker spinlock used to serialize access to
         * the worker's private counters */
        SpinlockInit(&workerPrivateData->PrivateSpinlock);
        /* Initialize per-worker counter used to synchronize reporting
         * to the test runner */
        Atomic64Init(&workerPrivateData->OngoingAccesses);
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
            PutRuntimeMemory(sharedMem);
            /* Free the private data of the worker whose deployment failed */
            PutRuntimeMemory(workerPrivateData);
            /* Fail the test synchronously */
            return -1;
        }
        s_workers.push_back(workerId);
    }

    /* Put the memory - each worker references it once, so the buffer
     * will be released only when all workers terminate at the end
     * of the test */
    PutRuntimeMemory(sharedMem);

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
    RefRuntimeMemory(workerData->SharedData);
    SetSharedData(workerData);

    return 0;
}

static void WorkerExit(void) {

    TestWorkerData * workerData = static_cast<TestWorkerData *>(GetSharedData());
    /* Decrement reference counter of the shared memory */
    PutRuntimeMemory(workerData->SharedData);
    /* Free private memory */
    PutRuntimeMemory(workerData);
}

static void WorkerBody(TMessage message) {

    AssertTrue(GetMessageId(message) == TRIGGER_MESSAGE_ID);

    TestWorkerData * data = static_cast<TestWorkerData *>(GetSharedData());

    /* Acquire a spinlock before accessing the RoundsRemaining field */
    SpinlockAcquire(&data->PrivateSpinlock);
    if (data->RoundsRemaining > 1) {

        data->RoundsRemaining--;

        /* We are about to make shared memory access - increment the
         * ongoing accesses counter before releasing the lock */
        Atomic64Inc(&data->OngoingAccesses);
        SpinlockRelease(&data->PrivateSpinlock);

        /* Resend the message back to self immediately */
        SendMessage(message, GetOwnWorkerId());

        /* Access the memory according to the test conditions */
        for (u32 i = 0; i < data->SharedData->LoopsPerRound; i++) {

            TouchData();
        }

        /* Allow another instance of an atomic worker to be scheduled in parallel -
         * this way we can test that EM's atomic processing works as expected and
         * serves as an alternative mean of synchronization */
        EndAtomicContext();

        /* Current execution path is done accessing the memory under test
         * - decrement the counter */
        Atomic64Dec(&data->OngoingAccesses);

    } else if (data->RoundsRemaining == 1) {

        /* Last round - do the memory access and finish the test on the current worker */

        data->RoundsRemaining--;
        /* Note that we could increment the ongoing accesses counter here, but we don't
         * need to since no other instance of the current worker can be executing this
         * code in parallel and it is only here that we are checking the counter */
        SpinlockRelease(&data->PrivateSpinlock);

        /* Finally destroy the message */
        DestroyMessage(message);

        /* Access the memory according to the test conditions */
        for (u32 i = 0; i < data->SharedData->LoopsPerRound; i++) {

            TouchData();
        }

        /* We do not call EndAtomicContext at this point since this is the last time
         * an atomic worker will make access to the shared memory */

        /* The worker may be parallel, so before declaring our job done wait for
         * any parallel executions to complete */
        while (Atomic64Get(&data->OngoingAccesses) > 0) {
            ;
        }

        /* Check if we are the last worker to terminate - only the last worker
         * reports the result to the test runner */
        if (Atomic64SubReturn(&data->SharedData->WorkersRemaining, 1) == 0) {

            /* If we are here, then all other workers have definitely terminated as
             * well as other copies (execution paths) of the current worker */

            /* Check the result */
            u64 counterValue = ReadFinalCounterValue();
            if (counterValue == data->SharedData->ExpectedValue) {

                TestCase::ReportTestResult(TestCase::Result::Success);

            } else {

                LogPrint(ELogSeverityLevel_Warning, "Incorrect counter value: %ld, expected: %ld", \
                    counterValue, data->SharedData->ExpectedValue);
                TestCase::ReportTestResult(TestCase::Result::Failure, \
                    "Incorrect counter value: %ld, expected: %ld", \
                    counterValue, data->SharedData->ExpectedValue);
            }
        }

    } else {

        /* Zero rounds remaining - destroy the message and wait for termination */
        SpinlockRelease(&data->PrivateSpinlock);
        DestroyMessage(message);
    }
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
