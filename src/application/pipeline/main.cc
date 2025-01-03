#include <menabrea/common.h>
#include "pipeline.hh"
#include "signer.hh"
#include "man_in_the_middle.hh"
#include "verifier.hh"
#include "ingress.hh"
#include <menabrea/exception.h>

static TWorkerId s_signerId;

APPLICATION_GLOBAL_INIT() {

    /* Run a dummy encrypted pipeline:
     * (1) Read in data from an MMU-accelerated ringbuffer (mring) and encrypt it.
     * (2) Decrypt, sign the data, reencrypt and send to a man in the middle
     * (3) Treat the ciphertext bits as two fixed-point fractions and interpret them as coordinates
     *     of a point in the unit square. If they are farther than 1 from the origin, modify the
     *     message to have the signature verification fail. Reencrypt and forward to the verifier.
     * (3) Decrypt, verify the signature and throw away the message. Count invalid signatures
     *     and derive the constant pi based on their ratio.
     */

    AssertTrue(0 == PipelineGlobalInit());
    TWorkerId verifierId = VerifierGlobalInit();
    AssertTrue(verifierId != WORKER_ID_INVALID);
    TWorkerId mitmId = ManInTheMiddleGlobalInit(verifierId);
    AssertTrue(mitmId != WORKER_ID_INVALID);
    s_signerId = SignerGlobalInit(mitmId);
    AssertTrue(s_signerId != WORKER_ID_INVALID);
    AssertTrue(0 == IngressGlobalInit());
}

APPLICATION_LOCAL_INIT(core) {

    IngressLocalInit(core, s_signerId);
}

APPLICATION_LOCAL_EXIT(core) {

    IngressLocalExit(core);
}

APPLICATION_GLOBAL_EXIT() {

    IngressGlobalExit();
    SignerGlobalExit();
    ManInTheMiddleGlobalExit();
    VerifierGlobalExit();
    PipelineGlobalExit();
}
