#include "man_in_the_middle.hh"
#include "pipeline.hh"
#include <menabrea/messaging.h>
#include <menabrea/cores.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

static TWorkerId s_workerId = WORKER_ID_INVALID;

static bool FixedPointInUnitCircle(void * entropy, u32 length) {

    /* This check should have already been made */
    AssertTrue(length >= 8);

    /* Interpret 64 or 128 bits at entropy as two fixed-point coordinates in the range [0, 1).
     * Return true if they give a point inside the unit circle and false otherwise. */
    if (length < 16) {

        u32 * entropy32 = static_cast<u32 *>(entropy);
        float x = static_cast<float>(entropy32[0]) / 4294967296.0f;
        float y = static_cast<float>(entropy32[1]) / 4294967296.0f;
        return x*x + y*y <= 1.0f;

    } else {

        u64 * entropy64 = static_cast<u64 *>(entropy);
        double x = static_cast<double>(entropy64[0]) / 18446744073709551616.0;
        double y = static_cast<double>(entropy64[1]) / 18446744073709551616.0;
        return x*x + y*y <= 1.0;
    }
}

static int MalloryInit(void * initArg) {

    SetSharedData(initArg);
    return 0;
}

static void MalloryBody(TMessage message) {

    TWorkerId nextStageId = reinterpret_cast<uintptr_t>(GetSharedData());
    PipelineMessage * payload = static_cast<PipelineMessage *>(GetMessagePayload(message));
    if (PipelineDecrypt(payload->Signature, payload->Signature, sizeof(payload->Signature) + payload->DataLen)) {
        LogPrint(ELogSeverityLevel_Warning, "Decryption failed for message from 0x%x of length %d", \
            GetMessageSender(message), payload->DataLen);
        DestroyMessage(message);
        return;
    }
    payload->DataLen -= CIPHER_TAILROOM;
    /* Decrypted ok, verify the signature */
    if (PipelineVerify(payload->Signature, payload->Data, payload->DataLen)) {
        LogPrint(ELogSeverityLevel_Warning, "Signature verification failed for message from 0x%x of length %d", \
            GetMessageSender(message), payload->DataLen);
        DestroyMessage(message);
        return;
    }

    if (payload->DataLen < 8) {
        LogPrint(ELogSeverityLevel_Info, "Message too short to read fixed-point numbers");
        DestroyMessage(message);
        return;
    }

    if (!FixedPointInUnitCircle(payload->Data, payload->DataLen)) {

        /* Point outside the unit circle, mangle the message to have the signature fail verification */
        payload->Data[0] += 1;
    }

    /* Reencrypt the message together with the (now possibly invalid) signature */
    PipelineEncrypt(payload->Signature, payload->Signature, sizeof(payload->Signature) + payload->DataLen);
    payload->DataLen += CIPHER_TAILROOM;
    SendMessage(message, nextStageId);
}

TWorkerId ManInTheMiddleGlobalInit(TWorkerId nextStageId) {

    LogPrint(ELogSeverityLevel_Debug, "Initializing the man-in-the-middle service...");
    SWorkerConfig workerConfig = {
        .Name = "PipelineMallory",
        .InitArg = reinterpret_cast<void *>(static_cast<uintptr_t>(nextStageId)),
        .WorkerId = WORKER_ID_INVALID,
        .CoreMask = GetAllCoresMask(),
        .Parallel = true,
        .UserInit = MalloryInit,
        .WorkerBody = MalloryBody
    };
    s_workerId = DeployWorker(&workerConfig);
    return s_workerId;
}

void ManInTheMiddleGlobalExit(void) {

    TerminateWorker(s_workerId);
}
