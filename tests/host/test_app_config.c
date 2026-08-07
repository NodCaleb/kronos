#include "unity.h"
#include "app_config.h"
#include <string.h>

/* Build a fully valid config; individual tests mutate one field at a time. */
static device_config_t make_valid_config(void)
{
    device_config_t c;
    memset(&c, 0, sizeof(c));
    strncpy(c.wifi_ssid,            "MySSID",                     sizeof(c.wifi_ssid) - 1);
    strncpy(c.wifi_password,        "MyPass",                     sizeof(c.wifi_password) - 1);
    strncpy(c.ntp_server,           "pool.ntp.org",               sizeof(c.ntp_server) - 1);
    strncpy(c.tz_posix,             "UTC0",                       sizeof(c.tz_posix) - 1);
    strncpy(c.weather_api_url,      "https://api.example.com",    sizeof(c.weather_api_url) - 1);
    strncpy(c.weather_api_key,      "validkey123",                sizeof(c.weather_api_key) - 1);
    strncpy(c.weather_location,     "48.8566,2.3522",             sizeof(c.weather_location) - 1);
    c.weather_refresh_interval_s = 300;
    c.request_timeout_ms         = 10000;
    c.wifi_max_retry_interval_s  = 60;
    c.units                      = UNITS_METRIC;
    c.log_level                  = 3;
    return c;
}

/* Fully valid config returns ESP_OK. */
static void test_valid_config_returns_ok(void)
{
    device_config_t c = make_valid_config();
    TEST_ASSERT_EQUAL(ESP_OK, app_config_validate(&c));
}

/* Empty wifi_ssid returns ESP_ERR_INVALID_ARG. */
static void test_empty_ssid_rejected(void)
{
    device_config_t c = make_valid_config();
    c.wifi_ssid[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_config_validate(&c));
}

/* weather_api_url starting with http:// (not https://) returns ESP_ERR_INVALID_ARG. */
static void test_http_url_rejected(void)
{
    device_config_t c = make_valid_config();
    strncpy(c.weather_api_url, "http://api.example.com", sizeof(c.weather_api_url) - 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_config_validate(&c));
}

/* weather_refresh_interval_s = 59 (below minimum of 60) returns ESP_ERR_INVALID_ARG. */
static void test_refresh_interval_below_min_rejected(void)
{
    device_config_t c = make_valid_config();
    c.weather_refresh_interval_s = 59;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_config_validate(&c));
}

/* weather_refresh_interval_s = 3601 (above maximum of 3600) returns ESP_ERR_INVALID_ARG (SC-007). */
static void test_refresh_interval_above_max_rejected(void)
{
    device_config_t c = make_valid_config();
    c.weather_refresh_interval_s = 3601;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_config_validate(&c));
}

/* request_timeout_ms = 31000 (above maximum of 30000) returns ESP_ERR_INVALID_ARG. */
static void test_timeout_above_max_rejected(void)
{
    device_config_t c = make_valid_config();
    c.request_timeout_ms = 31000;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_config_validate(&c));
}

/* wifi_max_retry_interval_s = 301 (above maximum of 300) returns ESP_ERR_INVALID_ARG (SC-004). */
static void test_retry_interval_above_max_rejected(void)
{
    device_config_t c = make_valid_config();
    c.wifi_max_retry_interval_s = 301;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_config_validate(&c));
}

void run_app_config_tests(void)
{
    RUN_TEST(test_valid_config_returns_ok);
    RUN_TEST(test_empty_ssid_rejected);
    RUN_TEST(test_http_url_rejected);
    RUN_TEST(test_refresh_interval_below_min_rejected);
    RUN_TEST(test_refresh_interval_above_max_rejected);
    RUN_TEST(test_timeout_above_max_rejected);
    RUN_TEST(test_retry_interval_above_max_rejected);
}
