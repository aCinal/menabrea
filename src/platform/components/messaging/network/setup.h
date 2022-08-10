
#ifndef PLATFORM_COMPONENTS_MESSAGING_NETWORK_SETUP_H
#define PLATFORM_COMPONENTS_MESSAGING_NETWORK_SETUP_H

#include <menabrea/workers.h>
#include <event_machine.h>
#include <net/if.h>

typedef struct SNetworkingConfig {
    em_pool_cfg_t PacketPoolConfig; /* TODO: Use this to configure the packet pool */
    TWorkerId NodeId;
    char DeviceName[IFNAMSIZ];
} SNetworkingConfig;

void MessagingNetworkInit(SNetworkingConfig * config);
void MessagingNetworkTeardown(void);

#endif /* PLATFORM_COMPONENTS_MESSAGING_NETWORK_SETUP_H */
