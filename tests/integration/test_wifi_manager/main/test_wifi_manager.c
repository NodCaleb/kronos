#include "unity.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "app_state.h"

void setUp(void)    {}
void tearDown(void) {}

/* T062 — wifi_manager_init() returns ESP_OK given a valid device_config_t */
static void test_wifi_manager_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, app_state_init());

    device_config_t config;
    TEST_ASSERT_EQUAL(ESP_OK, app_config_load(&config));

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_init(&config));
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wifi_manager_init_returns_ok);
    UNITY_END();
}
