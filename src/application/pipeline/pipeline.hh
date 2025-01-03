
#ifndef __SRC_APPLICATION_PIPELINE_PIPELINE_H
#define __SRC_APPLICATION_PIPELINE_PIPELINE_H

#include <menabrea/messaging.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>

constexpr const TMessageId PIPELINE_MESSAGE = 0xFE3C;
/* Leave tailroom for the 16 bytes of a Poly1305 tag and 24 bytes of an XChaCha20 nonce */
constexpr const u32 CIPHER_TAG_SIZE = crypto_aead_xchacha20poly1305_ietf_ABYTES;
constexpr const u32 CIPHER_NONCE_SIZE = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
constexpr const u32 CIPHER_TAILROOM = CIPHER_TAG_SIZE + CIPHER_NONCE_SIZE;

struct PipelineMessage {
    u32 DataLen;
    u8 Signature[64];
    u8 Data[0];
};

#define __check_result __attribute__((warn_unused_result))

int PipelineGlobalInit(void);
void PipelineGlobalExit(void);
void PipelineSign(u8 * signature, const u8 * message, u32 length);
__check_result int PipelineVerify(const u8 * signature, const u8 * message, u32 length);
void PipelineEncrypt(u8 * ciphertext, const u8 * plaintext, u32 length);
__check_result int PipelineDecrypt(u8 * plaintext, const u8 * ciphertext, u32 length);

#endif /* __SRC_APPLICATION_PIPELINE_PIPELINE_H */
