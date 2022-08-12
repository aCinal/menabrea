#include <messaging/local/buffering.h>
#include <messaging/message.h>
#include <workers/worker_table.h>
#include <menabrea/exception.h>

int BufferMessage(TMessage message) {

    /* Caller must ensure synchronization */

    TWorkerId receiver = GetMessageReceiver(message);
    SWorkerContext * receiverContext = FetchWorkerContext(receiver);
    /* Assert function called in the correct context */
    AssertTrue(receiverContext->State == EWorkerState_Deploying);

    /* Search the worker's event buffer for a free slot */
    for (int i = 0; i < MESSAGE_BUFFER_LENGTH; i++) {

        if (receiverContext->MessageBuffer[i] == EM_EVENT_UNDEF) {

            /* Free slot found, save the message and return */
            receiverContext->MessageBuffer[i] = message;
            /* Success */
            return 0;
        }
    }

    /* Buffer full */
    return -1;
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
