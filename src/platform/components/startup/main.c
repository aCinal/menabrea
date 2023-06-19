#include <startup/command_line.h>
#include <startup/cpus.h>
#include <startup/open_data_plane_startup.h>
#include <startup/event_machine_startup.h>
#include <startup/load_applications.h>
#include <startup/event_dispatcher.h>
#include <log/log.h>
#include <log/startup_logger.h>
#include <exception/signal_handlers.h>
#include <exception/recovery.h>
#include <menabrea/log.h>
#include <string.h>
#include <stdlib.h>

#define LOG_VERBOSITY_ENV  "LOG_VERBOSE"

static void InitializeLogger(void);
static em_pool_cfg_t TranslateToEmPoolConfig(SPoolConfig * config, em_event_type_t eventType);

int main(int argc, char **argv) {

    InitializeLogger();
    InstallSignalHandlers();

    /* Parse command line arguments */
    SStartupParams * startupParams = ParseCommandLine(argc, argv);

    /* Allow running on all cores temporarily - ODP likes to verify CPU mask
     * configuration against the output of sched_getaffinity(), so during
     * startup make it so that it returns all available cores */
    int numOfCpus = ClaimAllCpus();

    /* Configure and initialize the ODP layer */
    SOdpStartupConfig odpStartupConfig = {
        .Cores = numOfCpus
    };
    odp_instance_t odpInstance = InitializeOpenDataPlane(&odpStartupConfig);

    /* Configure and initialize the OpenEM layer */
    SEmStartupConfig emStartupConfig = {
        .Cores = numOfCpus,
        .DefaultPoolConfig = TranslateToEmPoolConfig(&startupParams->DefaultPoolConfig, EM_EVENT_TYPE_SW)
    };
    em_conf_t * emConf = InitializeEventMachine(&emStartupConfig);

    /* Load applications before the fork to ensure identical layout of
     * address spaces in all children */
    SAppLibsSet * appLibs = LoadApplicationLibraries();

    /* Set up the OpenEM dispatchers config */
    SEventDispatcherConfig dispatcherConfig = {
        .Cores = numOfCpus,
        .OdpInstance = odpInstance,
        .AppLibs = appLibs,
        .WorkersConfig = {
            .NodeId = startupParams->NodeId
        },
        .MessagingConfig = {
            .PoolConfig = TranslateToEmPoolConfig(&startupParams->MessagePoolConfig, EM_EVENT_TYPE_SW),
            .NetworkingConfig = {
                .NodeId = startupParams->NodeId,
                .PktioBufs = startupParams->PktioBufferCount
            }
        },
        .MemoryConfig = {
            .PoolConfig = TranslateToEmPoolConfig(&startupParams->MemoryPoolConfig, EM_EVENT_TYPE_SW)
        }
    };
    (void) strcpy(dispatcherConfig.MessagingConfig.NetworkingConfig.DeviceName, startupParams->NetworkInterface);
    /* Release the startup params as not needed anymore */
    ReleaseStartupParams(startupParams);

    /* Run the platform on top of EM dispatchers */
    RunEventDispatchers(&dispatcherConfig);

    UnloadApplicationLibraries(appLibs);
    /* Platform torn down, clean up OpenEM and ODP */
    TearDownEventMachine(emConf);
    TearDownOpenDataPlane(odpInstance);

    LogPrint(ELogSeverityLevel_Info, "Platform shutdown complete");
    /* Do not run any recovery scripts, shutdown was graceful */
    DeclareShutdownGraceful();

    return 0;
}

static void InitializeLogger(void) {

    /* Determine verbosity based on an environment variable to have the logger
     * up and running when parsing the command line */
    char * verbosity = getenv(LOG_VERBOSITY_ENV);
    bool verbose = verbosity && 0 == strcmp(verbosity, "1");
    LogInit(verbose);
    SetLoggerCallback(StartupLoggerCallback);
}

static em_pool_cfg_t TranslateToEmPoolConfig(SPoolConfig * config, em_event_type_t eventType) {

    /* A separate structure is used to store the EM pool config
     * as parsed from the command-line to ensure compiler warnings
     * when anyone tries to call em_pool_cfg_init() on this config.
     * This would trigger a segfault in libodp-linux. For this
     * reason this function must only be called after ODP has been
     * initialized. */
    em_pool_cfg_t emPoolConfig;
    em_pool_cfg_init(&emPoolConfig);
    emPoolConfig.event_type = eventType;
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
