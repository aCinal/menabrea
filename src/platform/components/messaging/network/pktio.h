
#ifndef PLATFORM_COMPONENTS_MESSAGING_NETWORK_PKTIO_H
#define PLATFORM_COMPONENTS_MESSAGING_NETWORK_PKTIO_H

#include <odp_api.h>
#include <event_machine.h>
#include <menabrea/common.h>

#define NETWORKING_PACKET_POOL  ( (em_pool_t) 11 )

void PktioInit(const char * ifName, u32 bufCount);
void PktioTeardown(void);
odp_queue_t GetPktoutQueue(void);
odp_pktin_queue_t GetPktinQueue(void);

#endif /* PLATFORM_COMPONENTS_MESSAGING_NETWORK_PKTIO_H */
