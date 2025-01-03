#include "ingress.hh"
#include "pipeline.hh"
#include <mring.h>
#include <menabrea/input.h>
#include <menabrea/log.h>
#include <menabrea/cores.h>
#include <menabrea/exception.h>

static void MringInputPoll(void * arg);

static struct mring * s_mring;
static TWorkerId s_receiverId;

static constexpr const u32 MAX_PAYLOAD_SIZE = 128;

int IngressGlobalInit(void) {

    LogPrint(ELogSeverityLevel_Debug, "Intializing the ingress service...");
    RegisterInputPolling(MringInputPoll, nullptr, GetAllCoresMask());
    return 0;
}

void IngressLocalInit(int core, TWorkerId receiverId) {

    LogPrint(ELogSeverityLevel_Debug, "Initializing mring ingress on core %d", core);
    s_receiverId = receiverId;
    /* Open a private mring on this core */
    s_mring = mring_open();
    if (s_mring) {

        LogPrint(ELogSeverityLevel_Info, "Mring opened successfully on core %d", core);

    } else {

        RaiseException(EExceptionFatality_Fatal, "Failed to open an mring on core %d", core);
    }
}

void IngressLocalExit(int core) {

    LogPrint(ELogSeverityLevel_Debug, "Mring ingress on core %d shutting down...", core);
    mring_close(s_mring);
}

void IngressGlobalExit(void) {

    /* Nothing to do here yet */
}

static void MringInputPoll(void * arg) {

    (void) arg;
    /* Access the mring private to this core */
    unsigned long readable;
    unsigned char * readBuffer = mring_read_begin(s_mring, &readable);

    if (readable > 0) {

        if (readable > MAX_PAYLOAD_SIZE) {

            readable = MAX_PAYLOAD_SIZE;
        }

        /* Allocate some extra room for the Poly1305 MAC and XChaCha20 nonce */
        TMessage message = CreateMessage(PIPELINE_MESSAGE, sizeof(PipelineMessage) + readable + CIPHER_TAILROOM);
        if (message != MESSAGE_INVALID) {

            PipelineMessage * payload = static_cast<PipelineMessage *>(GetMessagePayload(message));
            /* Encrypt the message and send to signer */
            PipelineEncrypt(payload->Data, readBuffer, readable);
            payload->DataLen = readable + CIPHER_TAILROOM;
            SendMessage(message, s_receiverId);

        } else {

            LogPrint(ELogSeverityLevel_Error, "Failed to create a pipeline message at ingress of payload size %ld", \
                readable);
        }
    }
    mring_read_end(s_mring, readable);
}
