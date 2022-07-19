set(SOURCES
    completion_daemon.c
    core_queue_groups.c
    messaging.c
    worker_table.c
    workers.c
)

add_library(work OBJECT ${SOURCES})
