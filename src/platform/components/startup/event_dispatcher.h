
#ifndef PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H
#define PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H

#include <startup/load_applications.h>
#include <workers/setup.h>
#include <messaging/setup.h>
#include <memory/setup.h>
#include <odp_api.h>
#include <event_machine.h>

typedef struct SEventDispatcherConfig {
    int Cores;
    odp_instance_t OdpInstance;
    SAppLibsSet * AppLibs;
    SWorkersConfig WorkersConfig;
    SMessagingConfig MessagingConfig;
    SMemoryConfig MemoryConfig;
} SEventDispatcherConfig;

void RunEventDispatchers(SEventDispatcherConfig * config);

#endif /* PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H */
