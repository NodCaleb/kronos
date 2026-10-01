#include "unity.h"
#include "display.h"

void setUp(void)    {}
void tearDown(void) {}

/* T069 — display_init() returns ESP_OK and does not crash (splash screen must appear) */
static void test_display_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, display_init());
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_display_init_returns_ok);
    UNITY_END();
}
