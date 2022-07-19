
#ifndef PLATFORM_COMPONENT_WORK_COMPLETION_DAEMON_H
#define PLATFORM_COMPONENT_WORK_COMPLETION_DAEMON_H

#include <event_machine.h>

void DeployCompletionDaemon(void);
em_queue_t GetCompletionDaemonQueue(void);
void CompletionDaemonTeardown(void);

#endif /* PLATFORM_COMPONENT_WORK_COMPLETION_DAEMON_H */
