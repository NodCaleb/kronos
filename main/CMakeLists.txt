idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        time_sync
        display
        app_config
        app_state
        error_handler
        # wifi_manager added in Phase 4 (T037)
        # weather_service added in Phase 6 (T044)
)
