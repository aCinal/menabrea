#include <messaging/network/router.h>
#include <messaging/network/pktio.h>
#include <messaging/network/translation.h>
#include <messaging/message.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/messaging.h>

#define CHAINING_QUEUE_NAME  "Event-Chaining-Output-00"

void RouteInternodeMessage(TMessage message) {

    static em_queue_t s_chainingQueue = EM_QUEUE_UNDEF;

    /* Do lazy queue lookup */
    if (unlikely(s_chainingQueue == EM_QUEUE_UNDEF)) {

        /* Look up EM chaining queue */
        s_chainingQueue = em_queue_find(CHAINING_QUEUE_NAME);
        AssertTrue(s_chainingQueue != EM_QUEUE_UNDEF);
    }

    if (unlikely(EM_OK != em_send(message, s_chainingQueue))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to route internode message 0x%x from 0x%x to 0x%x", \
            GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
        DestroyMessage(message);
    }
}

em_status_t event_send_device(em_event_t event, em_queue_t queue) {

    /* This function overrides a weakly-linked symbol in libemodp.so and gets
     * called when an event is pushed to the EM-chaining output queue */

    (void) queue;

    /* Create ODP packet based on the event */
    odp_packet_t packet = CreatePacketFromMessage(event);
    if (unlikely(packet == ODP_PACKET_INVALID)) {

        /* Failed to create the packet, it is the caller's responsibility
         * to free the event */
        return EM_ERROR;
    }

    AssertTrue(odp_packet_is_valid(packet));
    /* Send the event out an ODP queue */
    if (unlikely(0 != odp_queue_enq(GetPktoutQueue(), odp_packet_to_event(packet)))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to enqueue ODP packet");
        odp_packet_print(packet);
        odp_packet_free(packet);
    }

    return EM_OK;
}
