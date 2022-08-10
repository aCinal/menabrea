set(SOURCES
    setup.c
    timer_instance.c
    timer_table.c
    timing_daemon.c
    timing.c
)

add_library(timing OBJECT ${SOURCES})
