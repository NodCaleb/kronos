idf_component_register(
    SRCS "weather_service.c" "weather_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client espressif__cjson app_state app_config wifi_manager mbedtls
)
