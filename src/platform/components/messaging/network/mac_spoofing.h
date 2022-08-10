
#ifndef PLATFORM_COMPONENTS_MESSAGING_NETWORK_MAC_SPOOFING_H
#define PLATFORM_COMPONENTS_MESSAGING_NETWORK_MAC_SPOOFING_H

#include <menabrea/common.h>
#include <net/ethernet.h>

#define MAC_ADDR_LEN                 ETH_ALEN
#define MAC_ADDR_COMMON_BASE_BYTE_0  ( (u8) 0xDE )
#define MAC_ADDR_COMMON_BASE_BYTE_1  ( (u8) 0xAD )
#define MAC_ADDR_COMMON_BASE_BYTE_2  ( (u8) 0xBE )
#define MAC_ADDR_COMMON_BASE_BYTE_3  ( (u8) 0xEF )
#define MAC_ADDR_COMMON_BASE_BYTE_4  ( (u8) 0x42 )

void SetMacAddress(const char * ifName, const u8 * mac);
void GetMacAddress(const char * ifName, u8 * mac);

#endif /* PLATFORM_COMPONENTS_MESSAGING_NETWORK_MAC_SPOOFING_H */
