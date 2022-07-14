set(SOURCES
    core_queue_groups.c
    daemon.c
    messaging.c
    worker_table.c
    workers.c
)

add_library(work OBJECT ${SOURCES})
