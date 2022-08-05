
#ifndef PLATFORM_COMPONENTS_NETWORKING_ETH_IF_SETUP_H
#define PLATFORM_COMPONENTS_NETWORKING_ETH_IF_SETUP_H

#include <menabrea/common.h>

#define MAC_ADDR_LEN  6

void SetMacAddress(const char * ifName, const u8 * mac);
void GetMacAddress(const char * ifName, u8 * mac);

#endif /* PLATFORM_COMPONENTS_NETWORKING_ETH_IF_SETUP_H */
