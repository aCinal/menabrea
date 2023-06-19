set(SOURCES
    callstack.c
    exception.c
    recovery.c
    signal_handlers.c
)

add_library(exception OBJECT ${SOURCES})
