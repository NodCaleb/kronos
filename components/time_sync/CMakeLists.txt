idf_component_register(
    SRCS "time_format.c"
    INCLUDE_DIRS "include"
    REQUIRES app_config app_state
    # esp_sntp and wifi_manager added in Phase 5 when time_sync.c is implemented
)
