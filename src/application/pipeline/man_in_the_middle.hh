
#ifndef __SRC_APPLICATION_PIPELINE_MAN_IN_THE_MIDDLE_HH
#define __SRC_APPLICATION_PIPELINE_MAN_IN_THE_MIDDLE_HH

#include <menabrea/workers.h>

TWorkerId ManInTheMiddleGlobalInit(TWorkerId nextStageId);
void ManInTheMiddleGlobalExit(void);

#endif /* __SRC_APPLICATION_PIPELINE_MAN_IN_THE_MIDDLE_HH */
