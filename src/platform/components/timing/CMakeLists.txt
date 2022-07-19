set(SOURCES
    timing_daemon.c
    timer_table.c
    timing.c
)

add_library(timing OBJECT ${SOURCES})
