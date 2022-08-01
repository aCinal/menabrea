#include "startup_logger.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>

#define MAX_PROC_NAME_LEN  16

void StartupLoggerCallback(const char * severityTag, const char * timestamp, const char * userPayload) {

    char procName[MAX_PROC_NAME_LEN] = "<unknown>";
    /* Get process name */
    (void) prctl(PR_GET_NAME, procName, 0, 0, 0);

    fprintf(stderr, "%s %s %s (%d): %s\n", \
        severityTag, timestamp, procName, getpid(), userPayload);
}
