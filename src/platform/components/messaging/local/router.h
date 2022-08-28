
#ifndef PLATFORM_COMPONENTS_MESSAGING_LOCAL_ROUTER_H
#define PLATFORM_COMPONENTS_MESSAGING_LOCAL_ROUTER_H

#include <menabrea/messaging.h>

/**
 * @brief Route a message locally (within the current node)
 * @param message Message
 */
void RouteIntranodeMessage(TMessage message);

#endif /* PLATFORM_COMPONENTS_MESSAGING_LOCAL_ROUTER_H */
