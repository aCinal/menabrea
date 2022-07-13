#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <stdlib.h>
#include <time.h>

static void WorkerBody(TMessage message);
static TWorkerId GetRandomWorkerId(void);
static int GetRandomCoreMask(void);
static bool TimeForNewMessage(void);
static void MasterLocalInit(int core);

TWorkerId s_workerIdPool[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};

#define WORKER_POOL_SIZE  ( sizeof(s_workerIdPool) / sizeof(s_workerIdPool[0]) )

extern "C" void ApplicationGlobalInit(void) {

    srand(time(NULL));
    LogPrint(ELogSeverityLevel_Info, "%s() called. Deploying %ld workers", \
        __FUNCTION__, WORKER_POOL_SIZE);

    for (int i = 0; i < WORKER_POOL_SIZE; i++) {

        char name[16];
        (void) snprintf(name, sizeof(name), "Worker_%d", i);
        s_workerIdPool[i] = DeploySimpleWorker(name, s_workerIdPool[i], GetRandomCoreMask(), WorkerBody);
        AssertTrue(WORKER_ID_INVALID != s_workerIdPool[i]);
    }

    SWorkerConfig masterConfig = {
        .Name = "MasterTrigger",
        .WorkerId = 0xFF,
        .CoreMask = 0b0001,
        .Parallel = false,
        .UserLocalInit = MasterLocalInit,
        .WorkerBody = WorkerBody
    };
    DeployWorker(&masterConfig);
}

extern "C" void ApplicationLocalInit(int core) {

    LogPrint(ELogSeverityLevel_Info, "%s() called on core %d", __FUNCTION__, core);
}

extern "C" void ApplicationLocalExit(int core) {

    LogPrint(ELogSeverityLevel_Info, "%s() called on core %d", __FUNCTION__, core);
}

extern "C" void ApplicationGlobalExit(void) {

    LogPrint(ELogSeverityLevel_Info, "%s() called", __FUNCTION__);
}

static void WorkerBody(TMessage message) {

    TWorkerId self = GetOwnWorkerId();
    TWorkerId receiver = GetRandomWorkerId();

    if (TimeForNewMessage()) {

        /* Try creating a new message */
        TMessage newMessage = CreateMessage(0xABBA, 0);

        /* At some point pool should run out */
        if (unlikely(newMessage == MESSAGE_INVALID)) {

            /* Pool empty - log it and destroy the received message to free some buffers */
            LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received message 0x%x from 0x%x! Failed to allocate new message thus freeing the old one...", \
                self, GetMessageId(message), GetMessageSender(message));
            DestroyMessage(message);

        } else {

            TWorkerId newReceiver = GetRandomWorkerId();
            SendMessage(message, receiver);
            SendMessage(newMessage, newReceiver);
        }

    } else {

        /* Simply resend the message that was received */
        SendMessage(message, receiver);
    }
}

static TWorkerId GetRandomWorkerId(void) {

    return s_workerIdPool[rand() % WORKER_POOL_SIZE];
}

static int GetRandomCoreMask(void) {

    int modulus = 1 << em_core_count();
    int mask = rand() % modulus;    /* [0-15] */
    return mask ? mask : mask + 1;  /* [1-15] - exclude the empty set */
}

static bool TimeForNewMessage(void) {

    return rand() % 2;
}

static void MasterLocalInit(int core) {

    LogPrint(ELogSeverityLevel_Info, "%s() called on core. Creating a trigger message...", __FUNCTION__);
    TMessage message = CreateMessage(0xBEEF, 0);
    AssertTrue(message != MESSAGE_INVALID);
    SendMessage(message, GetRandomWorkerId());
}
