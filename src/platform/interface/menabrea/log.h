
#ifndef PLATFORM_INTERFACE_MENABREA_LOG_H
#define PLATFORM_INTERFACE_MENABREA_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/** @brief Log severity level */
typedef enum ELogSeverityLevel {
    ELogSeverityLevel_Critical = 0,  /**< Critical log */
    ELogSeverityLevel_Error,         /**< Error log */
    ELogSeverityLevel_Warning,       /**< Warning log */
    ELogSeverityLevel_Info,          /**< Info log */
    ELogSeverityLevel_Debug,         /**< Debug log (not printed unless in verbose mode) */

    ELogSeverityLevel_NumberOf,      /**< Number of valid enum values */
} ELogSeverityLevel;

/**
 * @brief Log a message
 * @param severity Severity level
 * @param format printf-like format string
 * @param ... Format arguments
 */
void LogPrint(ELogSeverityLevel severity, const char * format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief Log a message
 * @param severity Severity level
 * @param format printf-like format string
 * @param args Format arguments
 */
void LogPrintV(ELogSeverityLevel severity, const char * format, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_LOG_H */
