set(SOURCES
    mac_spoofing.c
    pktio.c
    router.c
    setup.c
    translation.c
)

add_library(messaging_network OBJECT ${SOURCES})
