#include "unity.h"
#include "time_sync.h"
#include "app_config.h"
#include "app_state.h"

void setUp(void)    {}
void tearDown(void) {}

/* T063 — time_sync_init() returns ESP_OK; state is TIME_STATE_NOT_SYNCED before any sync */
static void test_time_sync_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, app_state_init());

    device_config_t config;
    TEST_ASSERT_EQUAL(ESP_OK, app_config_load(&config));

    TEST_ASSERT_EQUAL(ESP_OK, time_sync_init(&config));
    TEST_ASSERT_EQUAL(TIME_STATE_NOT_SYNCED, time_sync_get_state());
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_time_sync_init_returns_ok);
    UNITY_END();
}
