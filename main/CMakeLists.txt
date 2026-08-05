idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        time_sync
        display
        app_config
        app_state
        error_handler
        wifi_manager
        # weather_service added in Phase 6 (T044)
)
