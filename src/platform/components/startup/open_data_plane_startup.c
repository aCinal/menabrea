
#include <startup/open_data_plane_startup.h>
#include <startup/cpus.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

static int OdpLogger(odp_log_level_t level, const char * format, ...);
static inline ELogSeverityLevel MapSeverityLevel(odp_log_level_t level);

odp_instance_t InitializeOpenDataPlane(SOdpStartupConfig * config) {

    LogPrint(ELogSeverityLevel_Info, "Initializing the ODP layer...");

    odp_cpumask_t workerMask;
    odp_cpumask_zero(&workerMask);
    for (int i = 1; i < config->Cores; i++) {

        odp_cpumask_set(&workerMask, i);
    }

    odp_cpumask_t controlMask;
    odp_cpumask_zero(&controlMask);
    odp_cpumask_set(&controlMask, 0);

    odp_init_t initParams;
    /* Set default param values */
    odp_init_param_init(&initParams);

    /* Assign roles to the cores */
    initParams.num_worker = odp_cpumask_count(&workerMask);
    initParams.worker_cpus = &workerMask;
    initParams.num_control = odp_cpumask_count(&controlMask);
    initParams.control_cpus = &controlMask;
    initParams.log_fn = OdpLogger;

    /* Set unused features to optimize performance */
    initParams.not_used.feat.cls = 1;       /* Classifier APIs */
    initParams.not_used.feat.compress = 1;  /* Compression APIs */
    initParams.not_used.feat.crypto = 1;    /* Crypto APIs */
    initParams.not_used.feat.dma = 1;       /* DMA APIs */
    initParams.not_used.feat.ipsec = 1;     /* IPsec APIs */
    initParams.not_used.feat.tm = 1;        /* Traffic manager APIs */

    /* Set the memory model */
    initParams.mem_model = ODP_MEM_MODEL_PROCESS;

    odp_instance_t instance;
    AssertTrue(0 == odp_init_global(&instance, &initParams, NULL));
    /* Pin the current process to core 0 temporarily - ODP relies on
     * sched_getcpu() to return a unique CPU id - if we happen to run
     * on a different core than 0 at the point of that call, then two
     * ODP threads (and later EM cores) will map to the same CPU in
     * ODP bookkeeping */
    PinCurrentProcessToCpu(0);
    AssertTrue(0 == odp_init_local(instance, ODP_THREAD_CONTROL));

    /* Configure the scheduler */
    odp_schedule_config_t schedulerConfig;
    odp_schedule_config_init(&schedulerConfig);
    /* EM does not need the ODP predefined scheduling groups */
    schedulerConfig.sched_group.all = 0;
    schedulerConfig.sched_group.control = 0;
    schedulerConfig.sched_group.worker = 0;
    AssertTrue(0 == odp_schedule_config(&schedulerConfig));

    /* Print ODP system info */
    odp_sys_info_print();

    LogPrint(ELogSeverityLevel_Info, "ODP layer ready");
    return instance;
}

void TearDownOpenDataPlane(odp_instance_t odpInstance) {

    AssertTrue(0 == odp_term_local());
    LogPrint(ELogSeverityLevel_Info, "Tearing down ODP globally...");
    AssertTrue(0 == odp_term_global(odpInstance));
}

static int OdpLogger(odp_log_level_t level, const char * format, ...) {

    va_list ap;
    va_start(ap, format);
    LogPrintV(MapSeverityLevel(level), format, ap);
    va_end(ap);
    return 0;
}

static inline ELogSeverityLevel MapSeverityLevel(odp_log_level_t level) {

    switch (level) {
    case ODP_LOG_DBG:

        return ELogSeverityLevel_Debug;

    case ODP_LOG_ERR:

        return ELogSeverityLevel_Error;

    default:

        return ELogSeverityLevel_Info;
    }
}
