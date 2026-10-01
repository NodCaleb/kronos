idf_component_register(
    SRCS "test_wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES unity wifi_manager app_config app_state
)
