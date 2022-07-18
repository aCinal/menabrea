#include <menabrea/messaging.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <menabrea/common.h>
#include "messaging.h"
#include "worker_table.h"
#include <event_machine.h>

static void PrintPoolConfig(em_pool_cfg_t * config);
static void RouteMessage(TMessage message);

void MessagingInit(em_pool_cfg_t * messagingPoolConfig) {

    /* Create custom event pool */
    AssertTrue(MESSAGING_EVENT_POOL == em_pool_create("comms_pool", MESSAGING_EVENT_POOL, messagingPoolConfig));
    LogPrint(ELogSeverityLevel_Info, "Successfully created event pool %" PRI_POOL " for messaging framework's use", \
        MESSAGING_EVENT_POOL);
    PrintPoolConfig(messagingPoolConfig);
}

void MessagingTeardown(void) {

    LogPrint(ELogSeverityLevel_Info, "%s(): Deleting the event pool...", __FUNCTION__);
    /* Delete the event pool */
    AssertTrue(EM_OK ==  em_pool_delete(MESSAGING_EVENT_POOL));
}

TMessage CreateMessage(TMessageId msgId, u32 payloadSize) {

    em_event_t event = em_alloc(sizeof(SMessageHeader) + payloadSize, EM_EVENT_TYPE_SW, MESSAGING_EVENT_POOL);

    if (likely(event != EM_EVENT_UNDEF)) {

        SMessage * msgData = (SMessage *) em_event_pointer(event);
        msgData->Header.MessageId = msgId;
        msgData->Header.PayloadSize = payloadSize;
        /* Only set the sender during the 'SendMessage' call */
        msgData->Header.Sender = WORKER_ID_INVALID;
        msgData->Header.Receiver = WORKER_ID_INVALID;
    }

    return event;
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

static TWorkerId GetMessageReceiver(TMessage message) {

    /* Utility function for internal use */
    SMessage * msgData = (SMessage *) em_event_pointer(message);
    return msgData->Header.Receiver;
}

void DestroyMessage(TMessage message) {

    em_free(message);
}

void SendMessage(TMessage message, TWorkerId receiver) {

    AssertTrue(receiver != WORKER_ID_INVALID);

    em_eo_t self = em_eo_current();

    /* Access the message based on the descriptor (event) */
    SMessage * msgData = (SMessage *) em_event_pointer(message);

    if (self != EM_EO_UNDEF) {

        SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
        /* Set the sender based on the current context */
        msgData->Header.Sender = context->WorkerId;

    } else {

        /* Allow sending a message from a non-EO context */
        msgData->Header.Sender = WORKER_ID_INVALID;
    }
    msgData->Header.Receiver = receiver;

    RouteMessage(message);
}

int FlushBufferedMessages(TWorkerId workerId) {

    /* Caller must ensure synchronization */

    int dropped = 0;
    SWorkerContext * context = FetchWorkerContext(workerId);
    /* Assert this function only gets called when the worker is
     * starting up */
    AssertTrue(context->State == EWorkerState_Deploying);

    for (int i = 0; i < MESSAGE_BUFFER_LENGTH && context->MessageBuffer[i] != MESSAGE_INVALID; i++) {

        TMessage message = context->MessageBuffer[i];
        if (unlikely(EM_OK != em_send(message, context->Queue))) {

            DestroyMessage(message);
            dropped++;
        }
        context->MessageBuffer[i] = MESSAGE_INVALID;
    }

    return dropped;
}

int DropBufferedMessages(TWorkerId workerId) {

    /* Caller must ensure synchronization */

    int dropped = 0;
    SWorkerContext * context = FetchWorkerContext(workerId);
    /* Assert this function only gets called when the worker is
     * starting up */
    AssertTrue(context->State == EWorkerState_Deploying);

    for (int i = 0; i < MESSAGE_BUFFER_LENGTH && context->MessageBuffer[i] != MESSAGE_INVALID; i++) {

        DestroyMessage(context->MessageBuffer[i]);
        context->MessageBuffer[i] = MESSAGE_INVALID;
        dropped++;
    }

    return dropped;
}

static void PrintPoolConfig(em_pool_cfg_t * config) {

    LogPrint(ELogSeverityLevel_Info, "Number of subpools: %d", config->num_subpools);
    for (int i = 0; i < config->num_subpools; i++) {

        LogPrint(ELogSeverityLevel_Info, "    subpool %d:", i);
        LogPrint(ELogSeverityLevel_Info, "        size: %d", config->subpool[i].size);
        LogPrint(ELogSeverityLevel_Info, "        num: %d", config->subpool[i].num);
        LogPrint(ELogSeverityLevel_Info, "        cache_size: %d", config->subpool[i].cache_size);
    }
}

static void RouteMessage(TMessage message) {

    SMessage * msgData = (SMessage *) em_event_pointer(message);
    TWorkerId receiver = msgData->Header.Receiver;

/* TODO: Implement me! */
#define IsLocalWorker(__worker) true
    if (IsLocalWorker(receiver)) {

        /* Lock the entry to ensure the queue is still valid when em_send() gets called */
        LockWorkerTableEntry(receiver);
        SWorkerContext * receiverContext = FetchWorkerContext(receiver);
        EWorkerState state = receiverContext->State;
        switch (state) {
        case EWorkerState_Active:
            /* Worker active - push the message to the EM queue */
            if (unlikely(EM_OK != em_send(message, receiverContext->Queue))) {

                UnlockWorkerTableEntry(receiver);
                LogPrint(ELogSeverityLevel_Error, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x)", \
                    GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
                /* We are still the owners of the message and must return it to the system */
                DestroyMessage(message);
                return;
            }
            UnlockWorkerTableEntry(receiver);
            break;

        case EWorkerState_Deploying:
            /* Worker still starting up - buffer the message */

            /* Search the worker's event buffer for a free slot */
            for (int i = 0; i < MESSAGE_BUFFER_LENGTH; i++) {

                if (receiverContext->MessageBuffer[i] == EM_EVENT_UNDEF) {

                    /* Free slot found, save the message and return */
                    receiverContext->MessageBuffer[i] = message;
                    UnlockWorkerTableEntry(receiver);
                    return;
                }
            }

            /* Failed to find a free slot, drop the message */
            UnlockWorkerTableEntry(receiver);
            LogPrint(ELogSeverityLevel_Warning, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x)" \
                " - deployment not yet complete and the message buffer is full", \
                GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
            DestroyMessage(message);
            break;

        default:
            UnlockWorkerTableEntry(receiver);
            LogPrint(ELogSeverityLevel_Warning, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x) - invalid receiver state: %d", \
                GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message), state);
            DestroyMessage(message);
            break;
        }

    } else {

        /* TODO: Implement EM chaining and find the relevant queue here */
        LogPrint(ELogSeverityLevel_Critical, "Routing to remote workers not supported at the moment!");
        DestroyMessage(message);
    }
}
