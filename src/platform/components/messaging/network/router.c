#include <messaging/network/router.h>
#include <messaging/network/pktio.h>
#include <messaging/network/translation.h>
#include <messaging/message.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <menabrea/messaging.h>

static int EmOutputFunction(const em_event_t events[], const unsigned int num, const em_queue_t outputQueue, void *outputFnArgs);

static em_queue_t s_outputQueue = EM_QUEUE_UNDEF;

void RouterInit(void) {

    LogPrint(ELogSeverityLevel_Info, "Creating the output queue...");

    em_output_queue_conf_t outputConfig = {
        .output_fn = EmOutputFunction
    };
    em_queue_conf_t queueConfig = {
        .flags = EM_QUEUE_FLAG_DEFAULT,
        .conf_len = sizeof(outputConfig),
        .conf = &outputConfig
    };
    /* Create the output queue */
    s_outputQueue = em_queue_create(
        "output_queue",
        EM_QUEUE_TYPE_OUTPUT,
        EM_QUEUE_PRIO_UNDEF,
        EM_QUEUE_GROUP_UNDEF,
        &queueConfig
    );
    AssertTrue(s_outputQueue != EM_QUEUE_UNDEF);
}

void RouteInternodeMessage(TMessage message) {

    if (unlikely(EM_OK != em_send(message, s_outputQueue))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to route internode message 0x%x from 0x%x to 0x%x", \
            GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
        DestroyMessage(message);
    }
}

static int EmOutputFunction(const em_event_t events[], const unsigned int num, const em_queue_t outputQueue, void *outputFnArgs) {

    (void) outputFnArgs;
    (void) outputQueue;

    /* em_send_multi() not used by the platform at the moment, use a simpler implementation */
    AssertTrue(1 == num);

    /* Create ODP packet based on the event */
    odp_packet_t packet = CreatePacketFromMessage(events[0]);
    if (unlikely(packet == ODP_PACKET_INVALID)) {

        /* Failed to create the packet, it is the caller's responsibility
         * to free the event */
        return 0;
    }

    AssertTrue(odp_packet_is_valid(packet));
    /* Send the event out an ODP queue */
    if (unlikely(0 != odp_queue_enq(GetPktoutQueue(), odp_packet_to_event(packet)))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to enqueue ODP packet");
        odp_packet_print(packet);
        odp_packet_free(packet);
        /* Let the caller free the input event */
        return 0;
    }

    /* CreatePacketFromMessage allocates a new event/packet from a separate pool.
     * Consume the input event. TODO: Study using a single pool with zero copy
     * and only mark the events as free from EM point POV via em_event_mark_free. */
    em_free_multi(events, num);
    return 1;
}
