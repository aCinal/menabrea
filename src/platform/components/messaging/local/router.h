
#ifndef PLATFORM_COMPONENTS_MESSAGING_LOCAL_ROUTER_H
#define PLATFORM_COMPONENTS_MESSAGING_LOCAL_ROUTER_H

#include <menabrea/messaging.h>

/**
 * @brief Route a message locally (within the current node)
 * @param message Message
 * @return Number of messages/events enqueued into EM, i.e. 1 on success, 0 on failure
 */
int RouteIntranodeMessage(TMessage message);

#endif /* PLATFORM_COMPONENTS_MESSAGING_LOCAL_ROUTER_H */
