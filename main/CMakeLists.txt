set(SOURCES "display.c"
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
    EMBED_FILES "assets/no_wifi.ebm" "assets/regist.ebm" "assets/emo_neutral.ebm" "assets/emo_wink.ebm" "assets/emo_happy1.ebm" "assets/emo_happy2.ebm" "assets/emo_sad1.ebm" "assets/emo_sad2.ebm" "assets/emo_angry1.ebm" "assets/emo_angry2.ebm" 
    EMBED_TXTFILES "assets/wifi_configuration_ap.html"
    REQUIRES 
        nvs_flash 
        button
        esp_http_client
        esp_http_server
        esp_wifi
        spi_flash 
        app_update 
        driver
        led_strip
        esp_adc
        esp_lcd
)
