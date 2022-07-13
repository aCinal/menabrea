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

    em_eo_t self = em_eo_current();
    /* Do not allow calling this function from a non-EO context */
    AssertTrue(self != EM_EO_UNDEF);
    SWorkerContext * context = (SWorkerContext *) em_eo_get_context(self);
    /* Access the message based on the descriptor (event) */
    SMessage * msgData = (SMessage *) em_event_pointer(message);
    /* Set the sender based on the current context */
    msgData->Header.Sender = context->WorkerId;
    msgData->Header.Receiver = receiver;

    RouteMessage(message);
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
        if (receiverContext->State == EWorkerState_Active) {

            if (unlikely(EM_OK != em_send(message, receiverContext->Queue))) {

                UnlockWorkerTableEntry(receiver);
                LogPrint(ELogSeverityLevel_Error, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x)", \
                    GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
                /* We are still the owners of the message and must return it to the system */
                DestroyMessage(message);
                return;
            }
            UnlockWorkerTableEntry(receiver);

        } else {

            EWorkerState state = receiverContext->State;
            UnlockWorkerTableEntry(receiver);
            LogPrint(ELogSeverityLevel_Warning, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x) - invalid receiver state: %d", \
                GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message), state);
            DestroyMessage(message);
        }

    } else {

        /* TODO: Implement EM chaining and find the relevant queue here */
        LogPrint(ELogSeverityLevel_Critical, "Routing to remote workers not supported at the moment!");
        DestroyMessage(message);
    }
}
