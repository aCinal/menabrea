#include "pipeline.hh"
#include <menabrea/log.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>
#include <sodium/crypto_sign_ed25519.h>
#include <sodium/randombytes.h>

/* XChaCha20-Poly1305 key */
static u8 s_symmetricKey[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
/* Ed25519 key pair */
static u8 s_signingKey[crypto_sign_ed25519_SECRETKEYBYTES];
static u8 s_validatingKey[crypto_sign_ed25519_PUBLICKEYBYTES];

int PipelineGlobalInit(void) {

    LogPrint(ELogSeverityLevel_Debug, "Generating pipeline keys...");
    /* Generate an authenticated encryption/decryption key */
    crypto_aead_xchacha20poly1305_ietf_keygen(s_symmetricKey);
    /* Generate a signing key pair */
    if (crypto_sign_ed25519_keypair(s_validatingKey, s_signingKey)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to generate an Ed25519 key pair");
        return -1;
    }

    return 0;
}

void PipelineGlobalExit(void) {

    /* Nothing to do here yet */
}

void PipelineSign(u8 * signature, const u8 * message, u32 length) {

    (void) crypto_sign_ed25519_detached(signature, nullptr, message, length, s_signingKey);
}

int PipelineVerify(const u8 * signature, const u8 * message, u32 length) {

    return crypto_sign_ed25519_verify_detached(
        signature,
        message,
        length,
        s_validatingKey
    );
}

void PipelineEncrypt(u8 * ciphertext, const u8 * plaintext, u32 length) {

    /* Append the nonce and the authentication tag to the ciphertext */
    u8 * nonce = ciphertext + length;
    u8 * tag = nonce + CIPHER_NONCE_SIZE;

    /* Generate a random nonce */
    randombytes_buf(nonce, CIPHER_NONCE_SIZE);

    (void) crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
        ciphertext,
        tag,
        nullptr,
        plaintext,
        length,
        nullptr,
        0,
        nullptr,
        nonce,
        s_symmetricKey
    );
}

int PipelineDecrypt(u8 * plaintext, const u8 * ciphertext, u32 length) {

    if (length < CIPHER_TAILROOM) {

        LogPrint(ELogSeverityLevel_Error, "Ciphertext too short: %d", length);
        return -1;
    }

    /* Compute the length of the encrypted message without nonces or tags */
    length -= CIPHER_TAILROOM;
    /* Find the nonce and the tag */
    const u8 * nonce = ciphertext + length;
    const u8 * tag = nonce + CIPHER_NONCE_SIZE;

    return crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
        plaintext,
        nullptr,
        ciphertext,
        length,
        tag,
        nullptr,
        0,
        nonce,
        s_symmetricKey
    );
}
