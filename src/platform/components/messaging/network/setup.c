#include <messaging/network/mac_spoofing.h>
#include <messaging/network/pktio.h>
#include <messaging/network/setup.h>
#include <messaging/network/translation.h>
#include <messaging/router.h>
#include <menabrea/input.h>
#include <menabrea/cores.h>

#define MAX_RX_BURST  32

static void NetworkInputPoll(void);

static u8 s_originalMacAddr[MAC_ADDR_LEN];
static char s_ifName[IFNAMSIZ];

void MessagingNetworkInit(SNetworkingConfig * config) {

    /* Store original MAC address and interface name (ensure proper NULL-termination) */
    GetMacAddress(config->DeviceName, s_originalMacAddr);
    (void) strncpy(s_ifName, config->DeviceName, sizeof(s_ifName) - 1);
    s_ifName[sizeof(s_ifName) - 1] = '\0';

    /* Set new address*/
    u8 macAddr[MAC_ADDR_LEN] = {
        MAC_ADDR_COMMON_BASE_BYTE_0,
        MAC_ADDR_COMMON_BASE_BYTE_1,
        MAC_ADDR_COMMON_BASE_BYTE_2,
        MAC_ADDR_COMMON_BASE_BYTE_3,
        MAC_ADDR_COMMON_BASE_BYTE_4,
        (u8)(config->NodeId & 0xFF)
    };
    SetMacAddress(config->DeviceName, macAddr);

    /* Initialize pktio */
    PktioInit(config->DeviceName, config->PktioBufs);

    /* Register an input poll callback */
    RegisterInputPolling(NetworkInputPoll, GetAllCoresMask());
}

void MessagingNetworkTeardown(void) {

    PktioTeardown();

    /* Restore original MAC address */
    SetMacAddress(s_ifName, s_originalMacAddr);
}

static void NetworkInputPoll(void) {

    odp_packet_t packets[MAX_RX_BURST];
    int packetsReceived = odp_pktin_recv(GetPktinQueue(), packets, MAX_RX_BURST);
    for (int i = 0; i < packetsReceived; i++) {

        odp_packet_t packet = packets[i];
        TMessage message = CreateMessageFromPacket(packet);
        if (likely(message != MESSAGE_INVALID)) {

            /* Route the event/message locally */
            RouteMessage(message);
        }

        odp_packet_free(packet);
    }
}
