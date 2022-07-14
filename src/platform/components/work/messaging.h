
#ifndef PLATFORM_COMPONENTS_WORK_MESSAGING_H
#define PLATFORM_COMPONENTS_WORK_MESSAGING_H

#include <menabrea/messaging.h>
#include <menabrea/common.h>
#include <event_machine.h>

typedef struct SMessageHeader {
    u32 PayloadSize;
    TWorkerId Sender;
    TWorkerId Receiver;
    TMessageId MessageId;
} SMessageHeader;

typedef struct SMessage {
    SMessageHeader Header;
    u8 UserPayload[0];
} SMessage;

#define MESSAGING_EVENT_POOL  ( (em_pool_t) 10 )

void MessagingInit(em_pool_cfg_t * messagingPoolConfig);
void MessagingTeardown(void);
int FlushBufferedMessages(TWorkerId workerId);

#endif /* PLATFORM_COMPONENTS_WORK_MESSAGING_H */
