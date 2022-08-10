set(SOURCES
    main.c
    command_line.c
    open_data_plane_startup.c
    event_machine_startup.c
    load_applications.c
    event_dispatcher.c
)

add_library(startup OBJECT ${SOURCES})
