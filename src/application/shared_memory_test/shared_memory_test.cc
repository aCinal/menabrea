#include <menabrea/common.h>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/cores.h>
#include <menabrea/memory.h>

static int WorkerInit(void);
static void WorkerLocalInit(int core);
static void WorkerLocalExit(int core);
static void WorkerBody(TMessage message);

#define TEST_MEMORY_CONTENT       "Testing, attention please!"
#define TEST_MEMORY_CONTENT_SIZE  sizeof(TEST_MEMORY_CONTENT)

static void ** s_pointySharedMemory = NULL;
static void * s_leakySharedMemory = NULL;

extern "C" void ApplicationGlobalInit(void) {

    SWorkerConfig config = {
        .Name = "memlord",
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .Parallel = false,
        .UserInit = WorkerInit,
        .UserLocalInit = WorkerLocalInit,
        .UserLocalExit = WorkerLocalExit,
        .WorkerBody = WorkerBody
    };

    (void) DeployWorker(&config);
}

extern "C" void ApplicationLocalInit(int core) {

    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

    /* Put the shared memory once for each core */
    for (int i = 0; i < em_core_count(); i++) {

        PutMemory(*s_pointySharedMemory);
        PutMemory(s_pointySharedMemory);
    }
}

static int WorkerInit(void) {

    /* Allocate memory for a pointer */
    s_pointySharedMemory = (void **) GetMemory(sizeof(void *));
    AssertTrue(s_pointySharedMemory != NULL);

    return 0;
}

static void WorkerLocalInit(int core) {

    if (core == 0) {

        LogPrint(ELogSeverityLevel_Info, "Core %d allocating additional shared memory after the fork...", core);

        /* On core 0 access the shared memory and write a pointer to it
         * - other cores then shall access this pointer and hopefully it
         * will be valid in their address spaces as well */
        *s_pointySharedMemory = GetMemory(TEST_MEMORY_CONTENT_SIZE);
        AssertTrue(*s_pointySharedMemory);

        (void) memcpy(*s_pointySharedMemory, TEST_MEMORY_CONTENT, TEST_MEMORY_CONTENT_SIZE);

        /* Also allocate a buffer with no intention of freeing it */
        s_leakySharedMemory = GetMemory(2 * TEST_MEMORY_CONTENT_SIZE);
        AssertTrue(s_leakySharedMemory);

    } else {

        /* Add references to the shared memory */
        RefMemory(s_pointySharedMemory);
    }

    /* Request termination of self */
    TerminateWorker(WORKER_ID_INVALID);
}

static void WorkerLocalExit(int core) {

    (void) core;

    if (core != 0) {

        LogPrint(ELogSeverityLevel_Info, "Core %d checking shared memory at %p...", core, *s_pointySharedMemory);
        /* Try using the pointer from a different address space */
        AssertTrue(0 == memcmp(*s_pointySharedMemory, TEST_MEMORY_CONTENT, TEST_MEMORY_CONTENT_SIZE));

        LogPrint(ELogSeverityLevel_Info, "Core %d upping the reference count...", core);
        /* Up the reference counter */
        RefMemory(*s_pointySharedMemory);
    }
}

static void WorkerBody(TMessage message) {

    RaiseException(EExceptionFatality_Fatal, 0, "Worker 0x%x received a message unexpectedly (message ID: 0x%x, sender: 0x%x)", \
        GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
}
