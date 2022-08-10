#include <messaging/network/translation.h>
#include <messaging/network/mac_spoofing.h>
#include <messaging/network/pktio.h>
#include <messaging/message.h>
#include <menabrea/workers.h>
#include <menabrea/messaging.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <odp/helper/odph_api.h>
#include <event_machine/platform/event_machine_odp_ext.h>

#define LLC_HEADER_LEN  4

typedef struct SLlcHeader {
    u8 Dsap;
    u8 Ssap;
    u16 Control;
} SLlcHeader;

ODP_STATIC_ASSERT(sizeof(SLlcHeader) == LLC_HEADER_LEN, \
    "Logical Link Control header size inconsistent");

static inline void FillInEthHeader(odp_packet_t packet, TWorkerId messageReceiver);
static inline void FillInLlcHeader(odp_packet_t packet);
static inline void CopyMessageData(odp_packet_t packet, TMessage message);
static inline bool IsValidEthHeader(odp_packet_t packet);
static inline bool IsValidLlcHeader(odp_packet_t packet);

odp_packet_t CreatePacketFromMessage(TMessage message) {

    /* Calculate total size of the message */
    u32 messageSize = GetMessagePayloadSize(message) + sizeof(SMessageHeader);
    /* Size of the Ethernet payload is the message plus an LLC header */
    u32 ethPayloadSize = messageSize + LLC_HEADER_LEN;
    /* Calculate total size of a packet (Ethernet frame) */
    u32 packetSize = ethPayloadSize + ODPH_ETHHDR_LEN;

    /* Allocate from EM pool mapped to ODP packet pool */
    em_event_t packetEvent = em_alloc(packetSize, EM_EVENT_TYPE_PACKET, NETWORKING_PACKET_POOL);
    if (unlikely(packetEvent == EM_EVENT_UNDEF)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to allocate ODP packet for outbound message 0x%x (sender: 0x%x, receiver: 0x%x)", \
            GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
        return ODP_PACKET_INVALID;
    }

    /* Convert EM event to ODP packet */
    odp_packet_t packet = odp_packet_from_event(em_odp_event2odp(packetEvent));
    /* Fill in the Ethernet and LLC headers */
    FillInEthHeader(packet, GetMessageReceiver(message));
    FillInLlcHeader(packet);
    /* Copy the message */
    CopyMessageData(packet, message);

    return packet;
}

TMessage CreateMessageFromPacket(odp_packet_t packet) {

    /* Get length of the packet */
    u32 packetLen = odp_packet_len(packet);
    /* The packet shouldn't have got here without a valid Ethernet header */
    AssertTrue(packetLen >= ODPH_ETHHDR_LEN);

    /* Validate the headers */
    if (unlikely(!IsValidEthHeader(packet) || !IsValidLlcHeader(packet))) {

        /* Invalid header(s) */
        return MESSAGE_INVALID;
    }

    /* Strip Ethernet and LLC headers */
    u32 dataLen = packetLen - ODPH_ETHHDR_LEN - LLC_HEADER_LEN;
    odph_ethhdr_t * ethHeader = odp_packet_data(packet);
    SLlcHeader * llcHeader = (SLlcHeader *)(ethHeader + 1);
    SMessage * messageData = (SMessage *)(llcHeader + 1);
    /* Validate the message */
    if (unlikely(!IsValidMessage(messageData, dataLen))) {

        LogPrint(ELogSeverityLevel_Warning, \
            "Malformed ODP packet from %02x:%02x:%02x:%02x:%02x:%02x - data len (no LLC): %d", \
            ethHeader->src.addr[0], ethHeader->src.addr[1], ethHeader->src.addr[2], \
            ethHeader->src.addr[3], ethHeader->src.addr[4], ethHeader->src.addr[5], \
            dataLen);
        return MESSAGE_INVALID;
    }

    TMessage message = CreateMessageFromBuffer(messageData);
    if (unlikely(message == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, \
            "Failed to allocate a local event for inbound packet (message ID: 0x%x, sender: 0x%x, receiver: 0x%x)", \
            messageData->Header.MessageId, messageData->Header.Sender, messageData->Header.Receiver);

        /* Fall through and return MESSAGE_INVALID */
    }

    return message;
}

static inline void FillInEthHeader(odp_packet_t packet, TWorkerId messageReceiver) {

    odph_ethhdr_t * eth = odp_packet_data(packet);

    /* Set the destination MAC address */
    eth->dst.addr[0] = MAC_ADDR_COMMON_BASE_BYTE_0;
    eth->dst.addr[1] = MAC_ADDR_COMMON_BASE_BYTE_1;
    eth->dst.addr[2] = MAC_ADDR_COMMON_BASE_BYTE_2;
    eth->dst.addr[3] = MAC_ADDR_COMMON_BASE_BYTE_3;
    eth->dst.addr[4] = MAC_ADDR_COMMON_BASE_BYTE_4;
    eth->dst.addr[5] = (u8) (WorkerIdGetNode(messageReceiver) & 0xFF);

    /* Set the source MAC address */
    eth->src.addr[0] = MAC_ADDR_COMMON_BASE_BYTE_0;
    eth->src.addr[1] = MAC_ADDR_COMMON_BASE_BYTE_1;
    eth->src.addr[2] = MAC_ADDR_COMMON_BASE_BYTE_2;
    eth->src.addr[3] = MAC_ADDR_COMMON_BASE_BYTE_3;
    eth->src.addr[4] = MAC_ADDR_COMMON_BASE_BYTE_4;
    eth->src.addr[5] = (u8) (GetOwnNodeId() & 0xFF);

    /* Set the payload length in network endianness */
    eth->type = odp_cpu_to_be_16(odp_packet_len(packet) - ODPH_ETHHDR_LEN);
}

static inline void FillInLlcHeader(odp_packet_t packet) {

    odph_ethhdr_t * eth = odp_packet_data(packet);
    SLlcHeader * llc = (SLlcHeader *)(eth + 1);

    /* NULL DSAP, SSAP, and control fields */
    llc->Dsap = 0;
    llc->Ssap = 0;
    llc->Control = 0;
}

static inline void CopyMessageData(odp_packet_t packet, TMessage message) {

    /* Skip the headers */
    odph_ethhdr_t * eth = odp_packet_data(packet);
    SLlcHeader * llc = (SLlcHeader *)(eth + 1);
    void * data = llc + 1;
    /* Get the bulk size of the message */
    u32 len = GetMessagePayloadSize(message) + sizeof(SMessageHeader);
    /* Copy into Ethernet/LLC data field */
    (void) memcpy(data, GetMessageData(message), len);
}

static inline bool IsValidEthHeader(odp_packet_t packet) {

    odph_ethhdr_t * eth = odp_packet_data(packet);
    return \
        /* Silently drop packets not originating from other nodes */
        eth->src.addr[0] == MAC_ADDR_COMMON_BASE_BYTE_0 && \
        eth->src.addr[1] == MAC_ADDR_COMMON_BASE_BYTE_1 && \
        eth->src.addr[2] == MAC_ADDR_COMMON_BASE_BYTE_2 && \
        eth->src.addr[3] == MAC_ADDR_COMMON_BASE_BYTE_3 && \
        eth->src.addr[4] == MAC_ADDR_COMMON_BASE_BYTE_4 && \
        eth->src.addr[5] <= MAX_NODE_ID && \
        /* Drop broadcast packets etc. */
        eth->dst.addr[0] == MAC_ADDR_COMMON_BASE_BYTE_0 && \
        eth->dst.addr[1] == MAC_ADDR_COMMON_BASE_BYTE_1 && \
        eth->dst.addr[2] == MAC_ADDR_COMMON_BASE_BYTE_2 && \
        eth->dst.addr[3] == MAC_ADDR_COMMON_BASE_BYTE_3 && \
        eth->dst.addr[4] == MAC_ADDR_COMMON_BASE_BYTE_4 && \
        eth->dst.addr[5] == (u8) (GetOwnNodeId() & 0xFF);
}

static inline bool IsValidLlcHeader(odp_packet_t packet) {

    odph_ethhdr_t * eth = odp_packet_data(packet);
    SLlcHeader * llc = (SLlcHeader *)(eth + 1);
    /* Make sure packet is long enough before accessing the LLC header */
    return odp_packet_len(packet) >= ODPH_ETHHDR_LEN + LLC_HEADER_LEN && \
        llc->Dsap == 0 && llc->Ssap == 0 && llc->Control == 0;
}
