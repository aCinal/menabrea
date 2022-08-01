set(SOURCES
    log.c
    runtime_logger.c
    startup_logger.c)

add_library(log OBJECT ${SOURCES})
