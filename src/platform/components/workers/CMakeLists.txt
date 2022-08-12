set(SOURCES
    completion_daemon.c
    setup.c
    worker_table.c
    workers.c
)

add_library(workers OBJECT ${SOURCES})
