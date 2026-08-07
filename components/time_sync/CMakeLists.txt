idf_component_register(
    SRCS "time_format.c" "time_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES app_config app_state lwip wifi_manager
)
