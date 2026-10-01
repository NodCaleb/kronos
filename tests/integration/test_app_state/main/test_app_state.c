#include "unity.h"
#include "app_state.h"
#include "freertos/FreeRTOS.h"

void setUp(void)    {}
void tearDown(void) {}

/* T070 — app_state_init() returns ESP_OK; immediate try_read is zero-initialised */
static void test_app_state_init_returns_zeroed_state(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, app_state_init());

    app_state_t snap;
    TEST_ASSERT_TRUE(app_state_try_read(&snap, portMAX_DELAY));
    TEST_ASSERT_EQUAL(WIFI_STATE_INIT, snap.wifi_state);
    TEST_ASSERT_EQUAL(TIME_STATE_NOT_SYNCED, snap.time_state);
    TEST_ASSERT_EQUAL(WEATHER_UNAVAILABLE, snap.weather.freshness);
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_app_state_init_returns_zeroed_state);
    UNITY_END();
}
