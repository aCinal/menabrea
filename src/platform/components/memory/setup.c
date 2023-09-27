#include <memory/setup.h>
#include <memory/memory.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

void MemorySetup(SMemoryConfig * config) {

    AssertTrue(RUNTIME_SHMEM_EVENT_POOL == em_pool_create("app_mem_pool", RUNTIME_SHMEM_EVENT_POOL, &config->PoolConfig));
    LogPrint(ELogSeverityLevel_Info, \
        "Successfully created event pool %" PRI_POOL " for satisfying runtime shared memory allocation requests", \
        RUNTIME_SHMEM_EVENT_POOL);
}

void MemoryTeardown(void) {

    LogPrint(ELogSeverityLevel_Info, "Deleting the runtime shared memory pool...");
    AssertTrue(EM_OK ==  em_pool_delete(RUNTIME_SHMEM_EVENT_POOL));
    ReleaseInitMemory();
}
