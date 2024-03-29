
#ifndef PLATFORM_COMPONENTS_MESSAGING_NETWORK_ROUTER_H
#define PLATFORM_COMPONENTS_MESSAGING_NETWORK_ROUTER_H

#include <menabrea/messaging.h>

void RouterInit(void);
void RouterTeardown(void);
void RouteInternodeMessage(TMessage message);

#endif /* PLATFORM_COMPONENTS_MESSAGING_NETWORK_ROUTER_H */
