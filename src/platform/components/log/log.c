
#include <log/log.h>
#include <menabrea/log.h>
#include <menabrea/common.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#define MAX_TIMESTAMP_LEN     64
#define MAX_USER_PAYLOAD_LEN  512

static bool s_verbose = false;
static TLoggerCallback s_loggerCallback = NULL;

void LogInit(bool verbose) {

    s_verbose = verbose;
}

void SetLoggerCallback(TLoggerCallback callback) {

    s_loggerCallback = callback;
}

void LogPrint(ELogSeverityLevel severity, const char * format, ...) {

    va_list ap;
    va_start(ap, format);
    LogPrintV(severity, format, ap);
    va_end(ap);
}

void LogPrintV(ELogSeverityLevel severity, const char * format, va_list args) {

    /* Do plain libc assert here to avoid a circular dependency */
    assert(ELogSeverityLevel_NumberOf > severity);
    assert(format != NULL);

    if (severity == ELogSeverityLevel_Debug && !s_verbose) {

        /* Debug log and verbose mode off - return as quickly as possible */
        return;
    }

    const char * severityTags[] = {
        [ELogSeverityLevel_Critical] = "[CRT]",  /* Not Chinese Remainder Theorem */
        [ELogSeverityLevel_Error] = "[ERR]",
        [ELogSeverityLevel_Warning] = "[WRN]",
        [ELogSeverityLevel_Info] = "[INF]",
        [ELogSeverityLevel_Debug] = "[DBG]"
    };

    char date[26];
    char timestamp[MAX_TIMESTAMP_LEN + 1];
    struct timespec ts;
    struct tm tm;
    /* Get current time */
    (void) clock_gettime(CLOCK_REALTIME, &ts);
    /* Parse the seconds into a human-readable date */
    (void) gmtime_r(&ts.tv_sec, &tm);
    /* Format the timestamp */
    size_t dateLen = strftime(date, sizeof(date), "%a %b %d %T", &tm);
    /* Assert the formatted date fits in the buffer */
    assert(dateLen > 0);
    size_t timestampLen = \
        snprintf(timestamp, sizeof(timestamp), "%s.%09ld", date, ts.tv_nsec);
    /* Assert enough space was reserved for the timestamp */
    assert(timestampLen <= MAX_TIMESTAMP_LEN);

    char userPayload[MAX_USER_PAYLOAD_LEN + 1];
    /* Format the user payload */
    size_t userPayloadLen = vsnprintf(userPayload, sizeof(userPayload), format, args);
    /* vsnprintf returns the number of bytes (characters) that could have been written
     * had the buffer been large enough. Take the possible truncation into account before
     * poking around in memory at e.g. userPayload[userPayloadLen]. */
    userPayloadLen = MIN(userPayloadLen, MAX_USER_PAYLOAD_LEN);

    /* Drop any newlines added by the caller */
    while (userPayload[userPayloadLen - 1] == '\n') {

        userPayloadLen = userPayloadLen - 1;
        if (userPayloadLen == 0) {
            /* Drop empty lines sometimes printed by ODP or EM */
            return;
        }
    }

    /* Ensure userPayload is properly null-terminated for fprintf's use */
    userPayload[userPayloadLen] = '\0';

    if (likely(s_loggerCallback)) {

        s_loggerCallback(severityTags[severity], timestamp, userPayload);

    } else {

        /* Default logging */
        fprintf(stderr, "%s %s: %s\n", \
            severityTags[severity], timestamp, userPayload);
    }
}
