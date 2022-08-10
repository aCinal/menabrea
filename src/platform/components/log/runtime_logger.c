#include <log/runtime_logger.h>
#include <event_machine.h>
#include <stdio.h>

void RuntimeLoggerCallback(const char * severityTag, const char * timestamp, const char * userPayload) {

    char eoName[EM_EO_NAME_LEN] = "--";

    em_eo_t currentEo = em_eo_current();
    if (currentEo != EM_EO_UNDEF) {

        (void) em_eo_get_name(currentEo, eoName, sizeof(eoName));
    }

    fprintf(stderr, "%s %s %s (disp_%d): %s\n", \
        severityTag, timestamp, eoName, em_core_id(), userPayload);
}
