
#ifndef PLATFORM_TEST_FRAMEWORK_LOGGING_HH
#define PLATFORM_TEST_FRAMEWORK_LOGGING_HH

#include <menabrea/log.h>

#ifndef FORMAT_LOG
    #define FORMAT_LOG(fmt)             "test_framework: " fmt
#endif /* FORMAT_LOG */

#define LOG_ERROR(fmt, ...)             LogPrint(ELogSeverityLevel_Error, FORMAT_LOG(fmt), ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)           LogPrint(ELogSeverityLevel_Warning, FORMAT_LOG(fmt), ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)              LogPrint(ELogSeverityLevel_Info, FORMAT_LOG(fmt), ##__VA_ARGS__)

#endif /* PLATFORM_TEST_FRAMEWORK_LOGGING_HH */
