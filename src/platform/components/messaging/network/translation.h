
#ifndef PLATFORM_COMPONENTS_MESSAGING_NETWORK_TRANSLATION_H
#define PLATFORM_COMPONENTS_MESSAGING_NETWORK_TRANSLATION_H

#include <menabrea/messaging.h>
#include <odp_api.h>

#define MAX_ETH_PACKET_SIZE  1500

odp_packet_t CreatePacketFromMessage(TMessage message);
TMessage CreateMessageFromPacket(odp_packet_t packet);

#endif /* PLATFORM_COMPONENTS_MESSAGING_NETWORK_TRANSLATION_H */
