
#ifndef PLATFORM_COMPONENTS_STARTUP_EVENT_MACHINE_STARTUP_H
#define PLATFORM_COMPONENTS_STARTUP_EVENT_MACHINE_STARTUP_H

#include <event_machine.h>

typedef struct SEmStartupConfig {
    int Cores;
    em_pool_cfg_t DefaultPoolConfig;
} SEmStartupConfig;

em_conf_t * InitializeEventMachine(SEmStartupConfig * config);
void TearDownEventMachine(em_conf_t * emConf);

#endif /* PLATFORM_COMPONENTS_STARTUP_EVENT_MACHINE_STARTUP_H */
