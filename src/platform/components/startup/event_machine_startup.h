
#ifndef PLATFORM_COMPONENTS_STARTUP_EVENT_MACHINE_STARTUP_H
#define PLATFORM_COMPONENTS_STARTUP_EVENT_MACHINE_STARTUP_H

#include <event_machine.h>

typedef struct SEmStartupConfig {
    const char * CoreMask;
    em_pool_cfg_t DefaultPoolConfig;
} SEmStartupConfig;

em_conf_t * InitializeEventMachine(SEmStartupConfig * config);

#endif /* PLATFORM_COMPONENTS_STARTUP_EVENT_MACHINE_STARTUP_H */
