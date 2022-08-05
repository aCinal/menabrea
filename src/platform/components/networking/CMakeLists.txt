set(SOURCES
    eth_if_setup.c
    networking.c
)

add_library(networking OBJECT ${SOURCES})
