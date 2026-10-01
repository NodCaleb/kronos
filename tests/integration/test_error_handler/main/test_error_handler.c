#include "unity.h"
#include "error_handler.h"

void setUp(void)    {}
void tearDown(void) {}

/* T071 — error_handler_warn() completes without triggering esp_restart() */
static void test_error_handler_warn_does_not_restart(void)
{
    error_handler_warn("TEST", "simulated transient error", ESP_ERR_TIMEOUT);
    /* Reaching this assertion proves error_handler_warn() returned normally */
    TEST_ASSERT_TRUE(true);
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_error_handler_warn_does_not_restart);
    UNITY_END();
}
