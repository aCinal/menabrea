#include <messaging/message.h>
#include <messaging/setup.h>

TMessage CreateMessage(TMessageId msgId, u32 payloadSize) {

    em_event_t event = em_alloc(MESSAGE_HEADER_LEN + payloadSize, EM_EVENT_TYPE_SW, MESSAGING_EVENT_POOL);

    if (likely(event != EM_EVENT_UNDEF)) {

        SMessage * msgData = (SMessage *) em_event_pointer(event);
        msgData->Header.MessageId = msgId;
        msgData->Header.PayloadSize = payloadSize;
        /* Only set the sender during the 'SendMessage' call */
        msgData->Header.Sender = WORKER_ID_INVALID;
        msgData->Header.Receiver = WORKER_ID_INVALID;
        msgData->Header.Magic = MESSAGE_HEADER_MAGIC;
        msgData->Header.Unused = 0;
    }

    return event;
}

TMessage CopyMessage(TMessage message) {

    return em_event_clone(message, EM_POOL_UNDEF);
}

void * GetMessagePayload(TMessage message) {

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    return (void *) &(msgData->UserPayload);
}

u32 GetMessagePayloadSize(TMessage message) {

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    return msgData->Header.PayloadSize;
}

TMessageId GetMessageId(TMessage message) {

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    return msgData->Header.MessageId;
}

TWorkerId GetMessageSender(TMessage message) {

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    return msgData->Header.Sender;
}

void DestroyMessage(TMessage message) {

    em_free(message);
}

SMessage * GetMessageData(TMessage message) {

    return (SMessage *) em_event_pointer(message);
}

TWorkerId GetMessageReceiver(TMessage message) {

    /* Utility function for internal use */

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    return msgData->Header.Receiver;
}

bool IsValidMessage(void * buffer, u32 size) {

    /* Check if a buffer, e.g. packet, has a structure of a message */
    if (unlikely(size < MESSAGE_HEADER_LEN)) {

        /* Not safe to access the message header */
        return false;
    }

    /* Safe to access the header */

    SMessage * msgData = (SMessage *) buffer;
    if (unlikely(msgData->Header.Magic != MESSAGE_HEADER_MAGIC)) {

        /* Invalid magic field */
        return false;
    }

    if (unlikely(size < msgData->Header.PayloadSize + MESSAGE_HEADER_LEN)) {

        /* Invalid payload size - allow buffer size to be larger, e.g. to support
         * Ethernet frame padding */
        return false;
    }

    TWorkerId receiver = msgData->Header.Receiver;
    if (unlikely(
        receiver == WORKER_ID_INVALID || \
        WorkerIdGetLocal(receiver) >= MAX_WORKER_COUNT || \
        WorkerIdGetNode(receiver) != GetOwnNodeId()
        )) {

        /* Invalid receiver */
        return false;
    }

    return true;
}

TMessage CreateMessageFromBuffer(void * buffer) {

    /* The caller must ensure the buffer is valid, e.g. by calling IsValidMessage(buffer, ...) */

    SMessage * srcData = (SMessage *) buffer;
    u32 payloadSize = srcData->Header.PayloadSize;

    TMessage message = CreateMessage(0, payloadSize);
    if (likely(message != MESSAGE_INVALID)) {

        SMessage * dstData = (SMessage *) em_event_pointer(message);
        dstData->Header = srcData->Header;
        (void) memcpy(dstData->UserPayload, srcData->UserPayload, payloadSize);
    }

    return message;
}
