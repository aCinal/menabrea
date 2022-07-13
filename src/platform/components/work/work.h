
#ifndef PLATFORM_COMPONENTS_WORKERS_WORK_H
#define PLATFORM_COMPONENTS_WORKERS_WORK_H

#include <event_machine.h>

typedef struct SWorkConfig {
    em_pool_cfg_t MessagingPoolConfig;
    int GlobalWorkerId;
} SWorkConfig;

void WorkInit(SWorkConfig * config);
void TerminateAllWorkers(void);
void WorkTeardown(void);

#endif /* PLATFORM_COMPONENTS_WORKERS_WORK_H */
