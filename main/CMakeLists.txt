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
    REQUIRES 
        nvs_flash 
        button 
        esp_http_client 
        esp_http_server
        esp_wifi        # 添加 WiFi 组件
        spi_flash      # 添加 Flash 组件
        app_update      # 添加这一行
        driver        # 添加 GPIO 驱动
        led_strip    # 添加 LED 控制
        esp_adc      # 添加 ADC 支持
)
