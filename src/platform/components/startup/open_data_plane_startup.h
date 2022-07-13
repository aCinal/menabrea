
#ifndef PLATFORM_COMPONENTS_STARTUP_OPEN_DATA_PLANE_STARTUP_H
#define PLATFORM_COMPONENTS_STARTUP_OPEN_DATA_PLANE_STARTUP_H

#include <odp_api.h>

typedef struct SOdpStartupConfig {
    int Cores;
} SOdpStartupConfig;

odp_instance_t InitializeOpenDataPlane(SOdpStartupConfig * config);

#endif /* PLATFORM_COMPONENTS_STARTUP_OPEN_DATA_PLANE_STARTUP_H */
