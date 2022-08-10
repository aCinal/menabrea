#include <work/setup.h>
#include <work/worker_table.h>
#include <work/completion_daemon.h>

void WorkInit(SWorkConfig * config) {

    WorkerTableInit(config->NodeId);
    DeployCompletionDaemon();
}

void WorkTeardown(void) {

    CompletionDaemonTeardown();
    WorkerTableTeardown();
}
