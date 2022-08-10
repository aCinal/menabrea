#include <messaging/network/mac_spoofing.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <string.h>

static void SetMacAddressImpl(const char * ifName, const u8 * mac);
static void IfDown(const char * ifName);
static void IfUp(const char * ifName);

void SetMacAddress(const char * ifName, const u8 * mac) {

    /* Set the MAC address of the target interface - note that this could also
     * be done by via a oneshot systemd service ran at startup, but it could
     * not be done via a .link file as these get parsed by udevd and due to a
     * race condition systemd-networkd ups the interface before udevd manages
     * to parse the config file and set the MAC address resulting in EBUSY. */

    IfDown(ifName);
    SetMacAddressImpl(ifName, mac);
    IfUp(ifName);
}

void GetMacAddress(const char * ifName, u8 * mac) {

    /* Open any socket as a channel to the kernel */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    AssertTrue(sock != -1);

    struct ifreq ifr;
    /* Copy the interface name and ensure proper NULL-termination */
    (void) strncpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    /* Obtain current HW address */
    AssertTrue(0 == ioctl(sock, SIOCGIFHWADDR, &ifr));

    /* Close the socket */
    AssertTrue(0 == close(sock));

    AssertTrue(mac != NULL);
    /* Copy the MAC address to the provided buffer */
    (void) memcpy(mac, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);
}

static void SetMacAddressImpl(const char * ifName, const u8 * mac) {

    LogPrint(ELogSeverityLevel_Info, \
        "Setting MAC address of '%s' to %02x:%02x:%02x:%02x:%02x:%02x...", \
        ifName, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* Open any socket as a channel to the kernel */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    AssertTrue(sock != -1);

    struct ifreq ifr;
    /* Copy the interface name and ensure proper NULL-termination */
    (void) strncpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    (void) memcpy(ifr.ifr_hwaddr.sa_data, mac, MAC_ADDR_LEN);

    /* Set the new HW address */
    AssertTrue(0 == ioctl(sock, SIOCSIFHWADDR, &ifr));

    /* Close the socket */
    AssertTrue(0 == close(sock));
}

static void IfDown(const char * ifName) {

    LogPrint(ELogSeverityLevel_Info, "Shutting down interface '%s' (temporarily)...", ifName);

    /* Open any socket as a channel to the kernel */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    AssertTrue(sock != -1);

    struct ifreq ifr;
    /* Copy the interface name and ensure proper NULL-termination */
    (void) strncpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    /* Get current flags */
    AssertTrue(0 == ioctl(sock, SIOCGIFFLAGS, &ifr));
    /* Clear the flag indicating the interface is up */
    ifr.ifr_flags &= ~IFF_UP;
    /* Set the new flags */
    AssertTrue(0 == ioctl(sock, SIOCSIFFLAGS, &ifr));

    /* Close the socket */
    AssertTrue(0 == close(sock));
}

static void IfUp(const char * ifName) {

    LogPrint(ELogSeverityLevel_Info, "Bringing interface '%s' up...", ifName);

    /* Open any socket as a channel to the kernel */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    AssertTrue(sock != -1);

    struct ifreq ifr;
    /* Copy the interface name and ensure proper NULL-termination */
    (void) strncpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    /* Get current flags */
    AssertTrue(0 == ioctl(sock, SIOCGIFFLAGS, &ifr));
    /* OR in flags indicating the interface is up */
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    /* Set the new flags */
    AssertTrue(0 == ioctl(sock, SIOCSIFFLAGS, &ifr));

    /* Close the socket */
    AssertTrue(0 == close(sock));
}
