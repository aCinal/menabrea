
#ifndef PLATFORM_INTERFACE_MENABREA_EXCEPTION_H
#define PLATFORM_INTERFACE_MENABREA_EXCEPTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/common.h>

/** @brief Exception fatality */
typedef enum EExceptionFatality {
    /**
     * @brief A non-fatal exception produces callstack dump and RaiseException then returns
     * @see RaiseException
     */
    EExceptionFatality_NonFatal,
    /**
     * @brief A fatal exception results in a SIGABRT being raised by the calling process and RaiseException does not return
     * @see RaiseException
     */
    EExceptionFatality_Fatal,
} EExceptionFatality;

/**
 * @brief Raise an exception
 * @param fatality Exception fatality
 * @param file Path to the source file where the exception is being raised
 * @param line Line in the code where the exception is being raised
 * @param function Name of the function which is raising the exception
 * @param message printf-like format string for the user message
 * @param ... Format arguments
 * @warning Do not use this directly, use RaiseException macro instead
 * @see RaiseException, EExceptionFatality
 */
void RaiseExceptionImpl(EExceptionFatality fatality, const char * file, int line, const char * function,
    const char * message, ...) __attribute__((format(printf, 5, 6)));

/**
 * @brief Convenience macro wrapper around RaiseExceptionImpl
 * @param fatality Exception fatality
 * @param message printf-like format string for the user message
 * @param ... Format string arguments
 * @see RaiseExceptionImpl
 */
#define RaiseException(fatality, message, ...) RaiseExceptionImpl((fatality), __FILE__, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)

/**
 * @brief Assert that an expression is true, raise fatal exception otherwise
 * @param expr Expression to evaluate
 */
#define AssertTrue(expr) \
    if (unlikely(!(expr))) { \
        RaiseException(EExceptionFatality_Fatal, "Assertion failed: " #expr ); \
    }

/**
 * @brief Register shell commands to be executed on disgraceful exit
 * @param cmd printf-like format string for the command
 * @param ... Format string arguments
 */
void OnDisgracefulShutdown(const char * cmd, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_EXCEPTION_H */
