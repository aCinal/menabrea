#define _GNU_SOURCE
#include "command_line.h"
#include "../log/log.h"
#include "../exception/signal_handlers.h"
#include "open_data_plane_startup.h"
#include "event_machine_startup.h"
#include "load_applications.h"
#include "event_dispatcher.h"
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <string.h>
#include <stdlib.h>

#define LOG_VERBOSITY_ENV  "LOG_VERBOSE"

static void InitializeLogger(void);
static int ClaimCpus(void);
static em_pool_cfg_t TranslateToEmPoolConfig(SPoolConfig * config);

/**
 * @brief Platform entry point
 * @param argc Arguments count
 * @param argv Arguments vector
 * @return Status
 */
int main(int argc, char **argv) {

    InitializeLogger();

    /* Install signal handlers */
    InstallSignalHandlers();

    /* Parse command line arguments */
    SStartupParams * startupParams = ParseCommandLine(argc, argv);

    /* Allow running on all cores */
    int numOfCpus = ClaimCpus();

    LogPrint(ELogSeverityLevel_Info, "Initializing ODP layer...");
    /* Configure and initialize the ODP layer */
    SOdpStartupConfig odpStartupConfig = {
        .Cores = numOfCpus,
    };
    odp_instance_t odpInstance = InitializeOpenDataPlane(&odpStartupConfig);
    LogPrint(ELogSeverityLevel_Info, "ODP layer ready");

    LogPrint(ELogSeverityLevel_Info, "Initializing the OpenEM layer...");
    /* Configure and initialize the OpenEM layer */
    SEmStartupConfig emStartupConfig = {
        .CoreMask = "0xF",
        .DefaultPoolConfig = TranslateToEmPoolConfig(&startupParams->DefaultPoolConfig)
    };
    em_conf_t * emConf = InitializeEventMachine(&emStartupConfig);
    LogPrint(ELogSeverityLevel_Info, "OpenEM layer ready");

    LogPrint(ELogSeverityLevel_Info, "Loading application libraries...");
    /* Load applications before the fork to ensure identical layout of
     * address spaces in all children */
    SAppLibsSet * appLibs = LoadApplicationLibraries();

    LogPrint(ELogSeverityLevel_Info, "Initiating dispatchers startup...");
    SEventDispatcherConfig dispatcherConfig = {
        .Cores = numOfCpus,
        .OdpInstance = odpInstance,
        .EmConf = emConf,
        .AppLibs = appLibs,
        .WorkConfig = {
            .MessagingPoolConfig = TranslateToEmPoolConfig(&startupParams->MessagingPoolConfig),
            .GlobalWorkerId = startupParams->GlobalWorkerId,
        }
    };
    RunEventDispatchers(&dispatcherConfig);

    AssertTrue(EM_OK == em_term(emConf));
    AssertTrue(0 == odp_term_local());
    LogPrint(ELogSeverityLevel_Info, "Main dispatcher shutdown complete");

    LogPrint(ELogSeverityLevel_Info, "Tearing down ODP globally...");
    AssertTrue(0 == odp_term_global(odpInstance));

    LogPrint(ELogSeverityLevel_Info, "Platform shutdown complete");

    /* Let the OS free emConf heap memory, unlink application libraries
     * and clean everything up. */
    return 0;
}

static void InitializeLogger(void) {

    /* Determine verbosity based on environment variable to have the logger
     * up and running when parsing the command line */
    char * verbosity = getenv(LOG_VERBOSITY_ENV);
    bool verbose = verbosity && 0 == strcmp(verbosity, "1");
    LogInit(verbose);
}

static int ClaimCpus(void) {

    cpu_set_t mask;
    CPU_ZERO(&mask);
    /* Enable running on all cores */
    int cores = get_nprocs();
    for (int i = 0; i < cores; i++) {

        CPU_SET(i, &mask);
    }

    AssertTrue(0 == sched_setaffinity(0, sizeof(cpu_set_t), &mask));

    return cores;
}

static em_pool_cfg_t TranslateToEmPoolConfig(SPoolConfig * config) {

    /* A separate structure is used to store the EM pool config
     * as parsed from the command-line to ensure compiler warnings
     * when anyone tries to call em_pool_cfg_init() on this config.
     * This would trigger a segfault in libodp-linux. For this
     * reason this function must only be called after ODP has been
     * initialized. */
    em_pool_cfg_t emPoolConfig;
    em_pool_cfg_init(&emPoolConfig);
    emPoolConfig.event_type = EM_EVENT_TYPE_SW;
    emPoolConfig.align_offset.in_use = 1;
    emPoolConfig.align_offset.value = 0;

    emPoolConfig.num_subpools = config->SubpoolCount;

    /* Translate the subpools config */
    for (int i = 0; i < config->SubpoolCount; i++) {

        emPoolConfig.subpool[i].size = config->Subpools[i].BufferSize;
        emPoolConfig.subpool[i].num = config->Subpools[i].NumOfBuffers;
        emPoolConfig.subpool[i].cache_size = config->Subpools[i].CacheSize;
    }

    return emPoolConfig;
}
