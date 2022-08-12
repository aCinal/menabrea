#include <messaging/local/router.h>
#include <messaging/local/buffering.h>
#include <messaging/message.h>
#include <menabrea/log.h>
#include <workers/worker_table.h>

int RouteIntranodeMessage(TMessage message) {

    TWorkerId receiver = GetMessageReceiver(message);

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
            /* No messages enqueued into EM */
            return 0;
        }
        UnlockWorkerTableEntry(receiver);
        /* Enqueued one message into EM */
        return 1;

    case EWorkerState_Deploying:
        /* Worker still starting up - buffer the message */
        if (unlikely(BufferMessage(message))) {

            /* Failed to find a free slot, drop the message */
            UnlockWorkerTableEntry(receiver);
            LogPrint(ELogSeverityLevel_Warning, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x)" \
                " - deployment not yet complete and the message buffer is full", \
                GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message));
            DestroyMessage(message);
            /* No messages enqueued into EM - return 0 */
            return 0;
        }

        UnlockWorkerTableEntry(receiver);
        /* No messages enqueued into EM */
        return 0;

    default:
        UnlockWorkerTableEntry(receiver);
        LogPrint(ELogSeverityLevel_Warning, "Failed to send message 0x%x (sender: 0x%x, receiver: 0x%x) - invalid receiver state: %d", \
            GetMessageId(message), GetMessageSender(message), GetMessageReceiver(message), state);
        DestroyMessage(message);
        /* No messages enqueued into EM */
        return 0;
    }
}
