
#ifndef PLATFORM_COMPONENTS_WORKERS_SETUP_H
#define PLATFORM_COMPONENTS_WORKERS_SETUP_H

#include <menabrea/workers.h>
#include <event_machine.h>

typedef struct SWorkersConfig {
    TWorkerId NodeId;
} SWorkersConfig;

void WorkersInit(SWorkersConfig * config);
void TerminateAllWorkers(void);
void WorkersTeardown(void);

#endif /* PLATFORM_COMPONENTS_WORKERS_SETUP_H */
