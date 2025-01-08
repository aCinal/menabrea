#include <messaging/router.h>
#include <messaging/local/router.h>
#include <messaging/network/router.h>
#include <messaging/message.h>
#include <workers/worker_table.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <event_machine.h>

void SendMessage(TMessage message, TWorkerId receiver) {

    if (unlikely(message == MESSAGE_INVALID)) {

        RaiseException(EExceptionFatality_NonFatal, \
            "Tried sending MESSAGE_INVALID to 0x%x", \
            receiver);
        return;
    }

    if (unlikely(receiver == WORKER_ID_INVALID || WorkerIdGetLocal(receiver) >= MAX_WORKER_COUNT || WorkerIdGetNode(receiver) < MIN_NODE_ID || WorkerIdGetNode(receiver) > MAX_NODE_ID)) {

        RaiseException(EExceptionFatality_NonFatal, \
            "Invalid receiver 0x%x of message 0x%x. Message not sent!", \
            receiver, GetMessageId(message));
        DestroyMessage(message);
        return;
    }

    em_eo_t self = em_eo_current();

    /* Access the message based on the descriptor (event) */
    SMessage * msgData = (SMessage *) em_event_pointer(message);

    if (self != EM_EO_UNDEF && NULL != em_eo_get_context(self)) {

        /* Sending a message from a worker context */

        SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
        /* Set the sender based on the current context */
        msgData->Header.Sender = context->WorkerId;

    } else {

        /* Allow sending a message from a non-EO context or from a raw EO
         * not associated with a worker context (used by platform internally) */
        msgData->Header.Sender = WORKER_ID_INVALID;
    }
    msgData->Header.Receiver = receiver;

    RouteMessage(message);
}

void RouteMessage(TMessage message) {

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    TWorkerId receiver = msgData->Header.Receiver;

    if (WorkerIdGetNode(receiver) == GetOwnNodeId()) {

        /* Worker local to the current node - enqueue the message via EM */

        RouteIntranodeMessage(message);

    } else {

        /* Remote worker - push the message out a network interface */

        RouteInternodeMessage(message);
    }
}
