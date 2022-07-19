
#ifndef PLATFORM_COMPONENTS_TIMING_TIMING_DAEMON_H
#define PLATFORM_COMPONENTS_TIMING_TIMING_DAEMON_H

#include <event_machine.h>

void DeployTimingDaemon(void);
em_queue_t GetTimingDaemonQueue(void);
void TimingDaemonTeardown(void);

#endif /* PLATFORM_COMPONENTS_TIMING_TIMING_DAEMON_H */
