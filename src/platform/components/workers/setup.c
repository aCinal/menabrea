#include <workers/setup.h>
#include <workers/worker_table.h>
#include <workers/completion_daemon.h>

void WorkersInit(SWorkersConfig * config) {

    WorkerTableInit(config->NodeId);
    DeployCompletionDaemon();
}

void WorkersTeardown(void) {

    CompletionDaemonTeardown();
    WorkerTableTeardown();
}
