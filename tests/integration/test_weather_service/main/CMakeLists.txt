idf_component_register(
    SRCS "test_weather_service.c"
    INCLUDE_DIRS "."
    REQUIRES unity weather_service app_config app_state
)
