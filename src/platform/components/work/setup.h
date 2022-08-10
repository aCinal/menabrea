
#ifndef PLATFORM_COMPONENTS_WORKERS_SETUP_H
#define PLATFORM_COMPONENTS_WORKERS_SETUP_H

#include <menabrea/workers.h>
#include <event_machine.h>

typedef struct SWorkConfig {
    TWorkerId NodeId;
} SWorkConfig;

void WorkInit(SWorkConfig * config);
void TerminateAllWorkers(void);
void WorkTeardown(void);

#endif /* PLATFORM_COMPONENTS_WORKERS_SETUP_H */
