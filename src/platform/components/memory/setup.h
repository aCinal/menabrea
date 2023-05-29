
#ifndef PLATFORM_COMPONENTS_MEMORY_SETUP_H
#define PLATFORM_COMPONENTS_MEMORY_SETUP_H

#include <event_machine.h>

typedef struct SMemoryConfig {
    em_pool_cfg_t PoolConfig;
} SMemoryConfig;

#define RUNTIME_SHMEM_EVENT_POOL  ( (em_pool_t) 12 )

void MemorySetup(SMemoryConfig * config);
void MemoryTeardown(void);

#endif /* PLATFORM_COMPONENTS_MEMORY_SETUP_H */
