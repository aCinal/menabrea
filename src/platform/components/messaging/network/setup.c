#include <messaging/network/mac_spoofing.h>
#include <messaging/network/pktio.h>
#include <messaging/network/router.h>
#include <messaging/network/setup.h>
#include <messaging/network/translation.h>
#include <messaging/router.h>
#include <menabrea/input.h>
#include <menabrea/cores.h>
#include <menabrea/exception.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#define MAX_RX_BURST  32
#define ORIGINAL_MAC_PATH  "/tmp/.original_mac"

static void SaveMacInFile(const char * filename, const u8 * mac);
static void NetworkInputPoll(void);

static u8 s_originalMacAddr[MAC_ADDR_LEN];
static char s_ifName[IFNAMSIZ];

void MessagingNetworkInit(SNetworkingConfig * config) {

    /* Store original MAC address and interface name (ensure proper NULL-termination) */
    GetMacAddress(config->DeviceName, s_originalMacAddr);
    (void) strncpy(s_ifName, config->DeviceName, sizeof(s_ifName) - 1);
    s_ifName[sizeof(s_ifName) - 1] = '\0';

    /* For emergency recovery purposes, store the MAC in a file as well */
    SaveMacInFile(ORIGINAL_MAC_PATH, s_originalMacAddr);
    /* On disgraceful shutdown restore the original MAC from the file... */
    OnDisgracefulShutdown("ifconfig %s down", config->DeviceName);
    OnDisgracefulShutdown("xargs ifconfig %s hw ether < %s", config->DeviceName, ORIGINAL_MAC_PATH);
    OnDisgracefulShutdown("ifconfig %s up", config->DeviceName);
    /* ...and clean up the file (if we crashed before registering this
     * the file would linger, but we do not care, it is overwritten each
     * time anyway) */
    OnDisgracefulShutdown("rm %s", ORIGINAL_MAC_PATH);

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

    /* Initialize the TX path */
    RouterInit();

    /* Register an input poll callback */
    RegisterInputPolling(NetworkInputPoll, GetAllCoresMask());
}

void MessagingNetworkTeardown(void) {

    RouterTeardown();
    PktioTeardown();
    /* Restore original MAC address */
    SetMacAddress(s_ifName, s_originalMacAddr);
    /* Remove the file storing it in case of disgraceful shutdown */
    AssertTrue(0 == unlink(ORIGINAL_MAC_PATH));
}

static void SaveMacInFile(const char * filename, const u8 * mac) {

    /* Open the file using open instead of fopen to have strict control over permissions */
    int fd = open(ORIGINAL_MAC_PATH, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
    AssertTrue(fd != -1);
    /* Write the MAC address to it */
    int charsWritten = dprintf(fd, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    AssertTrue(17 == charsWritten);
    /* Close the descriptor */
    AssertTrue(0 == close(fd));
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
