
#ifndef PLATFORM_COMPONENTS_WORKERS_WORK_H
#define PLATFORM_COMPONENTS_WORKERS_WORK_H

#include <menabrea/workers.h>
#include <event_machine.h>

typedef struct SWorkConfig {
    em_pool_cfg_t MessagingPoolConfig;
    TWorkerId NodeId;
} SWorkConfig;

void WorkInit(SWorkConfig * config);
void TerminateAllWorkers(void);
void WorkTeardown(void);

#endif /* PLATFORM_COMPONENTS_WORKERS_WORK_H */
