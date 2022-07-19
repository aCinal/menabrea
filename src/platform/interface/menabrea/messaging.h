
#ifndef PLATFORM_INTERFACE_MENABREA_COMMUNICATIONS_H
#define PLATFORM_INTERFACE_MENABREA_COMMUNICATIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/workers.h>

typedef u16 TMessageId;  /**< Message identifier type */

/**
 * @brief Create a message
 * @param msgId Identifier of the message (for application's use - transparent to the platform)
 * @param payloadSize Size of the user payload
 * @return Message handle or MESSAGE_INVALID on failure
 */
TMessage CreateMessage(TMessageId msgId, u32 payloadSize);

/**
 * @brief Create a copy of a message
 * @return Message handle or MESSAGE_INVALID on failure
 */
TMessage CopyMessage(TMessage message);

/**
 * @brief Access the payload of a message
 * @param message Message handle
 * @return Pointer to the message payload
 */
void * GetMessagePayload(TMessage message);

/**
 * @brief Get size of the message payload
 * @param message Message handle
 * @return Size of the payload in bytes
 */
u32 GetMessagePayloadSize(TMessage message);

/**
 * @brief Get identifier of a message
 * @param message Message handle
 * @return Message ID
 */
TMessageId GetMessageId(TMessage message);

/**
 * @brief Get worker ID of the message sender
 * @param message Message handle
 * @return Message sender's worker ID
 */
TWorkerId GetMessageSender(TMessage message);

/**
 * @brief Destroy a message
 * @param message Message handle
 * @warning The message handle must not be used after a call to this function
 */
void DestroyMessage(TMessage message);

/**
 * @brief Send a message to a worker
 * @param message Message handle
 * @param receiver Receiver's worker ID
 * @note After a call to this function, the ownership of the message is relinquished and the
 *       platform is responsible for the message delivery or destruction
 */
void SendMessage(TMessage message, TWorkerId receiver);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_COMMUNICATIONS_H */
