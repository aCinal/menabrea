
#ifndef DRIVERS_USER_INCLUDE_MRING_H
#define DRIVERS_USER_INCLUDE_MRING_H

#ifdef __cplusplus
extern "C" {
#endif

struct mring;

/**
 * @brief Open an mring device associated with the current core
 * @return Mring handle on success or NULL on failure
 */
struct mring *mring_open(void);

/**
 * @brief Close an mring device
 * @param mring Mring device handle
 */
void mring_close(struct mring *mring);

/**
 * @brief Start reading from an mring device
 * @param mring Mring device handle
 * @param readable Buffer where the number of readable bytes will be stored
 * @return Pointer to a read-only buffer from which the data can be read until a call to mring_read_end
 * @note Each call to mring_read_begin must be paired with a call to mring_read_end
 * @see mring_read_end
 */
unsigned char *mring_read_begin(struct mring *mring, unsigned long *readable);

/**
 * @brief Finish reading from an mring device
 * @param mring Mring device handle
 * @param bytes_read Number of bytes read
 * @note Each call to mring_read_end must be paired with an earlier call to mring_read_begin
 * @see mring_read_start
 */
void mring_read_end(struct mring *mring, unsigned long bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_USER_INCLUDE_MRING_H */
