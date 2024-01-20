
#ifndef PLATFORM_INTERFACE_MENABREA_INPUT_H
#define PLATFORM_INTERFACE_MENABREA_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback executed periodically in the dispatch loop
 * @note This function will be called only on cores on which input polling was registered
 * @warning This function must not use blocking system calls or delay for long
 * @see RegisterInputPolling
 */
typedef void (*TInputPollCallback)(void * arg);

/**
 * @brief Register a callback that will be called periodically to poll for external input
 * @param callback Input poll function
 * @param callbackArgument Argument passed by the platform to the input poll function being registered
 * @param coreMask Mask of cores on which the polling function should be called
 * @warning This function can only be called during the global application startup, i.e., before the fork
 * @warning The callback function registered here must not use blocking system calls or delay for long
 * @warning The callback will be executed on different cores depending on the core mask, and so the application
 *          must be cautious of passing pointers to core-private data as callback arguments
 * @see TInputPollCallback
 */
void RegisterInputPolling(TInputPollCallback callback, void * callbackArgument, int coreMask);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_INPUT_H */
