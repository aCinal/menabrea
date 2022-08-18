#include <memory/setup.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

void MemorySetup(SMemoryConfig * config) {

    AssertTrue(APP_MEMORY_EVENT_POOL == em_pool_create("app_mem_pool", APP_MEMORY_EVENT_POOL, &config->PoolConfig));
    LogPrint(ELogSeverityLevel_Info, "Successfully created event pool %" PRI_POOL " for satisfying application's allocation requests", \
        APP_MEMORY_EVENT_POOL);
}

void MemoryTeardown(void) {

    LogPrint(ELogSeverityLevel_Info, "Deleting the application's memory pool...");
    AssertTrue(EM_OK ==  em_pool_delete(APP_MEMORY_EVENT_POOL));
}
