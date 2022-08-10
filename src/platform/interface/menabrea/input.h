
#ifndef PLATFORM_INTERFACE_MENABREA_INPUT_H
#define PLATFORM_INTERFACE_MENABREA_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback executed periodically in the dispatch loop
 * @note The callback should return the number of messages sent from its context
 *       as a hint to the EM scheduler
 * @warning This function must not use blocking system calls or delay for long
 */
typedef int (*TInputPollCallback)(void);

/**
 * @brief Register a callback that will be called periodically to poll for external input
 * @param callback Input poll function
 * @param coreMask Mask of cores on which the polling function should be called
 * @warning This function can only be called during the global application startup, i.e. before the fork
 * @warning The callback function registered here must not use blocking system calls or delay for long
 * @see TInputPollCallback
 */
void RegisterInputPolling(TInputPollCallback callback, int coreMask);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_INPUT_H */
