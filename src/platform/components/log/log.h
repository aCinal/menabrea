
#ifndef PLATFORM_COMPONENTS_LOG_LOG_H
#define PLATFORM_COMPONENTS_LOG_LOG_H

#include <stdbool.h>

typedef void (* TLoggerCallback)(const char * severityTag, const char * timestamp, const char * userPayload);

void LogInit(bool verbose);
void SetLoggerCallback(TLoggerCallback callback);

#endif /* PLATFORM_COMPONENTS_LOG_LOG_H */
