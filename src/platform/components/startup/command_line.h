
#ifndef PLATFORM_COMPONENTS_STARTUP_COMMAND_LINE_H
#define PLATFORM_COMPONENTS_STARTUP_COMMAND_LINE_H

#include <menabrea/common.h>
#include <menabrea/workers.h>
#include <event_machine.h>

/* Use own structures for pool config so that no one tries to
 * call em_pool_cfg_init() on them. This leads to a segfault
 * in libodp-linux when called before ODP has been initialized. */

typedef struct SSubpoolConfig {
    u32 BufferSize;
    u32 NumOfBuffers;
    u32 CacheSize;
} SSubpoolConfig;

typedef struct SPoolConfig {
    u32 SubpoolCount;
    SSubpoolConfig Subpools[EM_MAX_SUBPOOLS];
} SPoolConfig;

typedef struct SStartupParams {
    SPoolConfig DefaultPoolConfig;
    SPoolConfig MessagingPoolConfig;
    TWorkerId GlobalWorkerId;
} SStartupParams;

SStartupParams * ParseCommandLine(int argc, char **argv);

#endif /* PLATFORM_COMPONENTS_STARTUP_COMMAND_LINE_H */
