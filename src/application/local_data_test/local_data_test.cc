#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/messaging.h>
#include <menabrea/exception.h>

static int WorkerInit(void);
static void MasterInit(int core);
static void WorkerLocalInit(int core);
static void WorkerBody(TMessage message);

#define SHARED_MEM_NAME    "local_data_test_shmem"
#define TEST_MESSAGE       "Come on Ace, we've got work to do"
#define TEST_MESSAGE_SIZE  sizeof(TEST_MESSAGE)


static TWorkerId s_workerId = WORKER_ID_INVALID;

extern "C" void ApplicationGlobalInit(void) {

    SWorkerConfig workerConfig = {
        .Name = "SimpleWorker",
        .WorkerId = 0x42,
        .CoreMask = 0b1111,
        .Parallel = true,
        .UserInit = WorkerInit,
        .UserLocalInit = WorkerLocalInit,
        .WorkerBody = WorkerBody
    };

    s_workerId = DeployWorker(&workerConfig);
    if (unlikely(WORKER_ID_INVALID == s_workerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to initialize the local data test worker");
    }

    workerConfig.Name = "MasterTrigger";
    workerConfig.WorkerId = WORKER_ID_INVALID;
    workerConfig.UserInit = NULL;
    workerConfig.UserLocalInit = MasterInit;
    workerConfig.WorkerBody = WorkerBody;
    workerConfig.CoreMask = 0b0001;

    TWorkerId masterId = DeployWorker(&workerConfig);
    if (unlikely(WORKER_ID_INVALID == masterId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the master trigger worker");
    }
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

}

static void MasterInit(int core) {

    (void) core;
    LogPrint(ELogSeverityLevel_Info, "Master trigger initializing. Sending the trigger message to 0x%x...", \
        s_workerId);

    TMessage message = CreateMessage(0xDEAD, TEST_MESSAGE_SIZE);
    AssertTrue(message != MESSAGE_INVALID);
    char * payload = static_cast<char *>(GetMessagePayload(message));
    (void) strncpy(payload, TEST_MESSAGE, TEST_MESSAGE_SIZE);
    SendMessage(message, s_workerId);
}

static int WorkerInit(void) {

    int core = em_core_id();
    void * sharedMemory = env_shared_reserve(SHARED_MEM_NAME, TEST_MESSAGE_SIZE);
    if (unlikely(sharedMemory == NULL)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocated shared memory. Marking worker startup as failed...");
        return -1;
    }

    char * textPayload = static_cast<char *>(sharedMemory);
    (void) strncpy(textPayload, TEST_MESSAGE, TEST_MESSAGE_SIZE);

    LogPrint(ELogSeverityLevel_Info, "Running global init on core %d. Setting local data...", core);
    SetLocalData(sharedMemory);
    return 0;
}

static void WorkerLocalInit(int core) {

    if (GetLocalData() == NULL) {

        void * sharedMemory = env_shared_lookup(SHARED_MEM_NAME);
        AssertTrue(sharedMemory != NULL);
        LogPrint(ELogSeverityLevel_Debug, "Setting local data on core %d...", core);
        SetLocalData(sharedMemory);

    } else {

        LogPrint(ELogSeverityLevel_Info, "Local data already set on core %d", core);
    }
}

static void WorkerBody(TMessage message) {

    static int counter = 0;

    const char * payload = static_cast<char *>(GetMessagePayload(message));
    if (counter % 10000 == 0) {

        LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received message: %s from 0x%x", \
            GetOwnWorkerId(), payload, GetMessageSender(message));
    }

    /* Verify the payload against local data */
    AssertTrue(0 == strcmp(payload, static_cast<char *>(GetLocalData())));

    /* Destroy the message and create a fresh one */
    DestroyMessage(message);
    TMessage response = CreateMessage(0xBEEF, TEST_MESSAGE_SIZE);
    AssertTrue(response != MESSAGE_INVALID);
    char * responsePayload = static_cast<char *>(GetMessagePayload(response));
    (void) strncpy(responsePayload, static_cast<char *>(GetLocalData()), TEST_MESSAGE_SIZE);

    /* Send the response back to self */
    SendMessage(response, GetOwnWorkerId());

    counter++;
}
