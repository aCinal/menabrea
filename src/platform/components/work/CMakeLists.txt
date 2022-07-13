set(SOURCES
    messaging.c
    core_queue_groups.c
    worker_table.c
    workers.c
)

add_library(work OBJECT ${SOURCES})
