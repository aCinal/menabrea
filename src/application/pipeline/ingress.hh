
#ifndef __SRC_APPLICATION_PIPELINE_INGRESS_HH
#define __SRC_APPLICATION_PIPELINE_INGRESS_HH

#include <menabrea/workers.h>

int IngressGlobalInit(void);
void IngressLocalInit(int core, TWorkerId receiverId);
void IngressLocalExit(int core);
void IngressGlobalExit(void);

#endif /* __SRC_APPLICATION_PIPELINE_INGRESS_HH */
