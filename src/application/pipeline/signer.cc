#include "signer.hh"
#include "pipeline.hh"
#include <menabrea/messaging.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>

static TWorkerId s_workerId = WORKER_ID_INVALID;

static int SignerInit(void * initArg) {

    SetSharedData(initArg);
    return 0;
}

static void SignerBody(TMessage message) {

    TWorkerId nextStageId = reinterpret_cast<uintptr_t>(GetSharedData());
    PipelineMessage * payload = static_cast<PipelineMessage *>(GetMessagePayload(message));
    if (PipelineDecrypt(payload->Data, payload->Data, payload->DataLen)) {
        LogPrint(ELogSeverityLevel_Warning, "Decryption failed for message from 0x%x", GetMessageSender(message));
        DestroyMessage(message);
        return;
    }
    payload->DataLen -= CIPHER_TAILROOM;
    /* Sign the plaintext */
    PipelineSign(payload->Signature, payload->Data, payload->DataLen);
    /* Reencrypt the signature together with the data */
    PipelineEncrypt(payload->Signature, payload->Signature, sizeof(payload->Signature) + payload->DataLen);
    payload->DataLen += CIPHER_TAILROOM;
    SendMessage(message, nextStageId);
}

TWorkerId SignerGlobalInit(TWorkerId nextStageId) {

    LogPrint(ELogSeverityLevel_Debug, "Initializing the signer service...");
    SWorkerConfig workerConfig = {
        .Name = "PipelineSigner",
        .InitArg = reinterpret_cast<void *>(static_cast<uintptr_t>(nextStageId)),
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .Parallel = true,
        .UserInit = SignerInit,
        .WorkerBody = SignerBody
    };
    s_workerId = DeployWorker(&workerConfig);
    return s_workerId;
}

void SignerGlobalExit(void) {

    TerminateWorker(s_workerId);
}
