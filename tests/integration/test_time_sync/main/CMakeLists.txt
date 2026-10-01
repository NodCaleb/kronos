idf_component_register(
    SRCS "test_time_sync.c"
    INCLUDE_DIRS "."
    REQUIRES unity time_sync app_config app_state
)
