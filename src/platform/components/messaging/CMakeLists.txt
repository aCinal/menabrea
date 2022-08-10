set(SOURCES
    message.c
    router.c
    setup.c
)

add_library(messaging_common OBJECT ${SOURCES})

add_subdirectory(local)
add_subdirectory(network)
