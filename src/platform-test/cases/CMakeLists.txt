set(SOURCES
    basic_timing/basic_timing.cc
    basic_workers/basic_workers.cc
    message_buffering/message_buffering.cc
    messaging_performance/messaging_performance.cc
    oneshot_timer/oneshot_timer.cc
    parallelism/parallelism.cc
    periodic_timer/periodic_timer.cc
    shared_memory/shared_memory.cc
)

add_library(cases OBJECT ${SOURCES})
