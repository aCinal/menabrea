
#ifndef PLATFORM_COMPONENTS_MESSAGING_SETUP_H
#define PLATFORM_COMPONENTS_MESSAGING_SETUP_H

#include <messaging/network/setup.h>
#include <event_machine.h>

#define MESSAGING_EVENT_POOL  ( (em_pool_t) 10 )

typedef struct SMessagingConfig {
    em_pool_cfg_t PoolConfig;
    SNetworkingConfig NetworkingConfig;
} SMessagingConfig;

void MessagingInit(SMessagingConfig * config);
void MessagingTeardown(void);

#endif /* PLATFORM_COMPONENTS_MESSAGING_SETUP_H */
