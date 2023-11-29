
#ifndef PLATFORM_COMPONENTS_MESSAGING_MESSAGE_H
#define PLATFORM_COMPONENTS_MESSAGING_MESSAGE_H

#include <menabrea/messaging.h>

#define MESSAGE_HEADER_MAGIC  ( (u16) 0xF321 )
#define MESSAGE_HEADER_LEN    16

typedef struct SMessageHeader {
    u32 PayloadSize;
    TWorkerId Sender;
    TWorkerId Receiver;
    TMessageId MessageId;
    u16 Magic;
    u32 Unused;  /* Pad to have the size be a multiple of 64 bits */
} SMessageHeader;

typedef struct SMessage {
    SMessageHeader Header;
    u8 UserPayload[0];
} SMessage;

ODP_STATIC_ASSERT(sizeof(SMessageHeader) == MESSAGE_HEADER_LEN, \
    "Message header length inconsistent");

SMessage * GetMessageData(TMessage message);
TWorkerId GetMessageReceiver(TMessage message);
bool IsValidMessage(void * buffer, u32 size);
TMessage CreateMessageFromBuffer(void * buffer);

#endif /* PLATFORM_COMPONENTS_MESSAGING_MESSAGE_H */
