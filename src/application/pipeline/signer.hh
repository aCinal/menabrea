
#ifndef __SRC_APPLICATION_PIPELINE_SIGNER_HH
#define __SRC_APPLICATION_PIPELINE_SIGNER_HH

#include <menabrea/workers.h>

TWorkerId SignerGlobalInit(TWorkerId nextStageId);
void SignerGlobalExit(void);

#endif /* __SRC_APPLICATION_PIPELINE_SIGNER_HH */
