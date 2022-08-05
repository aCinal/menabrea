#include "networking.h"
#include "eth_if_setup.h"
#include <net/if.h>

#define MAC_ADDR_COMMON_BASE_BYTE_0  ( (u8) 0xDE )
#define MAC_ADDR_COMMON_BASE_BYTE_1  ( (u8) 0xAD )
#define MAC_ADDR_COMMON_BASE_BYTE_2  ( (u8) 0xBE )
#define MAC_ADDR_COMMON_BASE_BYTE_3  ( (u8) 0xEF )
#define MAC_ADDR_COMMON_BASE_BYTE_4  ( (u8) 0x42 )

static u8 s_originalMacAddr[MAC_ADDR_LEN];
static char s_ifName[IFNAMSIZ];

void NetworkingInit(const char * ifName, int nodeId) {

    /* Store original MAC address and interface name (ensure proper NULL-termination) */
    GetMacAddress(ifName, s_originalMacAddr);
    (void) strncpy(s_ifName, ifName, sizeof(s_ifName) - 1);
    s_ifName[sizeof(s_ifName) - 1] = '\0';

    /* Set new address*/
    u8 macAddr[MAC_ADDR_LEN] = {
        MAC_ADDR_COMMON_BASE_BYTE_0,
        MAC_ADDR_COMMON_BASE_BYTE_1,
        MAC_ADDR_COMMON_BASE_BYTE_2,
        MAC_ADDR_COMMON_BASE_BYTE_3,
        MAC_ADDR_COMMON_BASE_BYTE_4,
        (u8)(nodeId & 0xFF)
    };
    SetMacAddress(ifName, macAddr);
}

void NetworkingTeardown(void) {

    /* Restore original MAC address */
    SetMacAddress(s_ifName, s_originalMacAddr);
}
