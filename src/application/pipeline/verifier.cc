#include "verifier.hh"
#include "pipeline.hh"
#include <menabrea/messaging.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>
#include <menabrea/memory.h>
#include <menabrea/exception.h>

static TWorkerId s_workerId = WORKER_ID_INVALID;

struct VerifierContext {
    TAtomic32 Valid;
    TAtomic32 Total;
};

static void VerifierBody(TMessage message) {

    VerifierContext * context = static_cast<VerifierContext *>(GetSharedData());
    if (unlikely(context == nullptr)) {
        LogPrint(ELogSeverityLevel_Debug, "Lazily initializing verifier context...");
        /* Lazily initialize our context while still running atomically */
        context = static_cast<VerifierContext *>(GetRuntimeMemory(sizeof(VerifierContext)));
        AssertTrue(context);
        /* Keep track of all messages received and those that had valid signatures */
        Atomic32Init(&context->Valid);
        Atomic32Init(&context->Total);
        SetSharedData(context);
    }
    /* Context valid, ok to schedule us in parallel, but read
     * the counters first, while still running atomically */
    u32 validCount = Atomic32Get(&context->Valid);
    u32 totalCount = Atomic32ReturnAdd(&context->Total, 1);
    EndAtomicContext();

    PipelineMessage * payload = static_cast<PipelineMessage *>(GetMessagePayload(message));
    if (PipelineDecrypt(payload->Signature, payload->Signature, sizeof(payload->Signature) + payload->DataLen)) {
        LogPrint(ELogSeverityLevel_Warning, "Decryption failed for message from 0x%x of length %d", \
            GetMessageSender(message), payload->DataLen);
        DestroyMessage(message);
        return;
    }
    payload->DataLen -= CIPHER_TAILROOM;
    /* Decrypted ok, verify the signature */
    if (0 == PipelineVerify(payload->Signature, payload->Data, payload->DataLen)) {

        Atomic32Inc(&context->Valid);
    }
    DestroyMessage(message);

    if (totalCount % 32 == 31) {
        /* Report the ratio before we made any updates */
        LogPrint(ELogSeverityLevel_Info, "Estimate of pi: %lf", \
            4.0 * validCount / totalCount);
    }
}

TWorkerId VerifierGlobalInit(void) {

    LogPrint(ELogSeverityLevel_Debug, "Initializing the verifier service...");
    s_workerId = DeploySimpleWorker("PipelineVerifier", WORKER_ID_INVALID, GetAllCoresMask(), VerifierBody);
    return s_workerId;
}

void VerifierGlobalExit(void) {

    TerminateWorker(s_workerId);
}
