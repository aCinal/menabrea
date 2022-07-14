
#ifndef PLATFORM_COMPONENT_WORK_DAEMON_H
#define PLATFORM_COMPONENT_WORK_DAEMON_H

#include <menabrea/workers.h>

/**
 * @brief Start the platform daemon
 */
void DeployDaemon(void);

/**
 * @brief Request completion of a worker deployment
 * @note Must be called from the context of the final local init of a worker
 */
void RequestDeploymentCompletion(void);

#endif /* PLATFORM_COMPONENT_WORK_DAEMON_H */
