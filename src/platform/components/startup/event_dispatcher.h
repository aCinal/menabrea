
#ifndef PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H
#define PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H

#include "load_applications.h"
#include <odp_api.h>
#include <event_machine.h>
#include "../work/work.h"

typedef struct SEventDispatcherConfig {
    int Cores;
    odp_instance_t OdpInstance;
    em_conf_t * EmConf;
    SAppLibsSet * AppLibs;
    SWorkConfig WorkConfig;
} SEventDispatcherConfig;

void RunEventDispatchers(SEventDispatcherConfig * config);

#endif /* PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H */
