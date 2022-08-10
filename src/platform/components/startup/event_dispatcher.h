
#ifndef PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H
#define PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H

#include <startup/load_applications.h>
#include <work/setup.h>
#include <messaging/setup.h>
#include <odp_api.h>
#include <event_machine.h>

typedef struct SEventDispatcherConfig {
    int Cores;
    odp_instance_t OdpInstance;
    em_conf_t * EmConf;
    SAppLibsSet * AppLibs;
    SWorkConfig WorkConfig;
    SMessagingConfig MessagingConfig;
} SEventDispatcherConfig;

void RunEventDispatchers(SEventDispatcherConfig * config);

#endif /* PLATFORM_COMPONENTS_STARTUP_EVENT_DISPATCHER_H */
