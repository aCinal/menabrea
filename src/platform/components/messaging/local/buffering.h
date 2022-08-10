
#ifndef PLATFORM_COMPONENTS_MESSAGING_LOCAL_BUFFERING_H
#define PLATFORM_COMPONENTS_MESSAGING_LOCAL_BUFFERING_H

#include <menabrea/messaging.h>

int BufferMessage(TMessage message);
int FlushBufferedMessages(TWorkerId workerId);
int DropBufferedMessages(TWorkerId workerId);

#endif /* PLATFORM_COMPONENTS_MESSAGING_LOCAL_BUFFERING_H */
