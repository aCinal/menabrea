set(SOURCES
    buffering.c
    router.c
)

add_library(messaging_local OBJECT ${SOURCES})
