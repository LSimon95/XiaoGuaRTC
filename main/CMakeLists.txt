set(SOURCES "led.c"
            "system_info.c"
            "configuration_ap.c"
            "network.c"
            "request.c"
            "chat.c"
            "audio.c"
            "main.c"
            )

set(INCLUDE_DIRS ".")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    EMBED_TXTFILES "assets/wifi_configuration_ap.html"
    )
