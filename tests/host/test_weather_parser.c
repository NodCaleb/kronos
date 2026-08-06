#include "unity.h"
#include "host_stubs.h"
#include "weather_service.h"
#include <string.h>

/* Minimal valid OWM One Call 3.0 JSON with exactly 6 hourly slots. */
static const char *VALID_JSON =
    "{"
    "  \"current\": {"
    "    \"temp\": 21.5,"
    "    \"weather\": [{\"id\": 801, \"description\": \"few clouds\"}]"
    "  },"
    "  \"hourly\": ["
    "    {\"dt\": 3600,  \"temp\": 19.0, \"weather\": [{\"id\": 500, \"description\": \"light rain\"}]},"
    "    {\"dt\": 7200,  \"temp\": 18.0, \"weather\": [{\"id\": 501, \"description\": \"moderate rain\"}]},"
    "    {\"dt\": 10800, \"temp\": 17.5, \"weather\": [{\"id\": 800, \"description\": \"clear sky\"}]},"
    "    {\"dt\": 14400, \"temp\": 16.0, \"weather\": [{\"id\": 802, \"description\": \"scattered clouds\"}]},"
    "    {\"dt\": 18000, \"temp\": 15.5, \"weather\": [{\"id\": 803, \"description\": \"broken clouds\"}]},"
    "    {\"dt\": 21600, \"temp\": 14.0, \"weather\": [{\"id\": 804, \"description\": \"overcast clouds\"}]}"
    "  ]"
    "}";

/* 1. Valid JSON parses correctly: current fields and 6 hourly slots. */
static void test_valid_json_parses_correctly(void)
{
    weather_data_t result;
    memset(&result, 0, sizeof(result));

    esp_err_t ret = weather_parse(VALID_JSON, strlen(VALID_JSON), &result);

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, result.current_temp);
    TEST_ASSERT_EQUAL(801, result.current_condition_code);
    TEST_ASSERT_EQUAL_STRING("few clouds", result.current_condition_text);
    TEST_ASSERT_EQUAL(6, result.forecast_count);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 19.0f, result.forecast[0].temperature);
    TEST_ASSERT_EQUAL(500, result.forecast[0].condition_code);
    TEST_ASSERT_EQUAL_STRING("light rain", result.forecast[0].condition_text);
    TEST_ASSERT_EQUAL(WEATHER_FRESH, result.freshness);
}

/* 2. Empty hourly array is rejected (zero-slot forecast is invalid per contract). */
static void test_empty_hourly_array_rejected(void)
{
    static const char *json =
        "{"
        "  \"current\": {"
        "    \"temp\": 20.0,"
        "    \"weather\": [{\"id\": 800, \"description\": \"clear sky\"}]"
        "  },"
        "  \"hourly\": []"
        "}";

    weather_data_t result;
    memset(&result, 0, sizeof(result));

    esp_err_t ret = weather_parse(json, strlen(json), &result);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, ret);
}

/* 3. Missing current object is rejected. */
static void test_missing_current_rejected(void)
{
    static const char *json =
        "{"
        "  \"hourly\": ["
        "    {\"dt\": 3600, \"temp\": 19.0, \"weather\": [{\"id\": 500, \"description\": \"rain\"}]}"
        "  ]"
        "}";

    weather_data_t result;
    memset(&result, 0, sizeof(result));

    esp_err_t ret = weather_parse(json, strlen(json), &result);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, ret);
}

/* 4. *out is unchanged on any parse failure (retain-old-data rule). */
static void test_out_unchanged_on_failure(void)
{
    weather_data_t result;
    memset(&result, 0, sizeof(result));
    result.current_temp   = 99.0f;
    result.forecast_count = 3;
    result.freshness      = WEATHER_STALE;

    esp_err_t ret = weather_parse("not valid json at all", 21, &result);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, ret);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.0f, result.current_temp);
    TEST_ASSERT_EQUAL(3, result.forecast_count);
    TEST_ASSERT_EQUAL(WEATHER_STALE, result.freshness);
}

/* 5. current.weather[0].id == 0 is rejected. */
static void test_condition_id_zero_rejected(void)
{
    static const char *json =
        "{"
        "  \"current\": {"
        "    \"temp\": 20.0,"
        "    \"weather\": [{\"id\": 0, \"description\": \"unknown\"}]"
        "  },"
        "  \"hourly\": ["
        "    {\"dt\": 3600, \"temp\": 19.0, \"weather\": [{\"id\": 500, \"description\": \"rain\"}]}"
        "  ]"
        "}";

    weather_data_t result;
    memset(&result, 0, sizeof(result));

    esp_err_t ret = weather_parse(json, strlen(json), &result);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, ret);
}

void run_weather_parser_tests(void)
{
    RUN_TEST(test_valid_json_parses_correctly);
    RUN_TEST(test_empty_hourly_array_rejected);
    RUN_TEST(test_missing_current_rejected);
    RUN_TEST(test_out_unchanged_on_failure);
    RUN_TEST(test_condition_id_zero_rejected);
}
