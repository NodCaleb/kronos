#include "unity.h"
#include "weather_service.h"
#include "app_config.h"
#include "app_state.h"

void setUp(void)    {}
void tearDown(void) {}

/* T064 — weather_service_init() returns ESP_OK; initial freshness is WEATHER_UNAVAILABLE */
static void test_weather_service_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, app_state_init());

    device_config_t config;
    TEST_ASSERT_EQUAL(ESP_OK, app_config_load(&config));

    app_state_lock();
    app_state_t *state = app_state_get();
    app_state_unlock();

    TEST_ASSERT_EQUAL(ESP_OK, weather_service_init(&config, state));

    app_state_t snap;
    TEST_ASSERT_TRUE(app_state_try_read(&snap, portMAX_DELAY));
    TEST_ASSERT_EQUAL(WEATHER_UNAVAILABLE, snap.weather.freshness);
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_weather_service_init_returns_ok);
    UNITY_END();
}
