
#include "event_machine_startup.h"
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <stdlib.h>

static int EmLogger(em_log_level_t level, const char * format, ...);
static int EmVLogger(em_log_level_t level, const char * format, va_list args);
static inline ELogSeverityLevel MapSeverityLevel(em_log_level_t level);

em_conf_t * InitializeEventMachine(SEmStartupConfig * config) {

    em_conf_t * emConf = malloc(sizeof(em_conf_t));
    AssertTrue(emConf != NULL);

    emConf->device_id = 0;
    /* Use process mode */
    emConf->thread_per_core = 0;
    emConf->process_per_core = 1;
    em_core_mask_zero(&emConf->phys_mask);
    (void) em_core_mask_set_str(config->CoreMask, &emConf->phys_mask);
    emConf->core_count = em_core_mask_count(&emConf->phys_mask);
    /* Enable event timer */
    emConf->event_timer = 1;
    emConf->default_pool_cfg = config->DefaultPoolConfig;

    emConf->log.log_fn = EmLogger;
    emConf->log.vlog_fn = EmVLogger;

    AssertTrue(EM_OK == em_init(emConf));

    return emConf;
}

static int EmLogger(em_log_level_t level, const char * format, ...) {

    va_list ap;
    va_start(ap, format);
    LogPrintV(MapSeverityLevel(level), format, ap);
    va_end(ap);
    return 0;
}

static int EmVLogger(em_log_level_t level, const char * format, va_list args) {

    LogPrintV(MapSeverityLevel(level), format, args);
    return 0;
}

static inline ELogSeverityLevel MapSeverityLevel(em_log_level_t level) {

    switch (level) {
    case EM_LOG_DBG:

        return ELogSeverityLevel_Debug;

    case EM_LOG_ERR:

        return ELogSeverityLevel_Error;

    default:

        return ELogSeverityLevel_Info;
    }
}
