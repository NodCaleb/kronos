#include "unity.h"

void setUp(void)    {}
void tearDown(void) {}

void run_weather_parser_tests(void);
void run_time_format_tests(void);
void run_app_config_tests(void);
void run_status_builder_tests(void);

int main(void)
{
    UNITY_BEGIN();
    run_weather_parser_tests();
    run_time_format_tests();
    run_app_config_tests();
    run_status_builder_tests();
    return UNITY_END();
}
