#include "unity.h"
#include "status_builder.h"
#include <string.h>

static app_state_t make_state(wifi_state_t wifi, time_sync_state_t time_s,
                               weather_freshness_t freshness)
{
    app_state_t s;
    memset(&s, 0, sizeof(s));
    s.wifi_state          = wifi;
    s.time_state          = time_s;
    s.weather.freshness   = freshness;
    return s;
}

/* 1. WIFI_STATE_INIT → "Wi-Fi: connecting" */
static void test_wifi_init_shows_connecting(void)
{
    app_state_t s = make_state(WIFI_STATE_INIT, TIME_STATE_NOT_SYNCED, WEATHER_UNAVAILABLE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Wi-Fi: connecting", buf);
}

/* 2. WIFI_STATE_CONNECTING → "Wi-Fi: connecting" */
static void test_wifi_connecting_shows_connecting(void)
{
    app_state_t s = make_state(WIFI_STATE_CONNECTING, TIME_STATE_NOT_SYNCED, WEATHER_UNAVAILABLE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Wi-Fi: connecting", buf);
}

/* 3. Connected + TIME_STATE_NOT_SYNCED → "Time: syncing" */
static void test_connected_no_time_shows_syncing(void)
{
    app_state_t s = make_state(WIFI_STATE_CONNECTED, TIME_STATE_NOT_SYNCED, WEATHER_UNAVAILABLE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Time: syncing", buf);
}

/* 4. Connected + synced + WEATHER_FRESH → "Synced" */
static void test_connected_synced_fresh_shows_synced(void)
{
    app_state_t s = make_state(WIFI_STATE_CONNECTED, TIME_STATE_SYNCED, WEATHER_FRESH);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Synced", buf);
}

/* 5. Connected + synced + WEATHER_STALE → "Weather stale" */
static void test_connected_synced_stale_shows_weather_stale(void)
{
    app_state_t s = make_state(WIFI_STATE_CONNECTED, TIME_STATE_SYNCED, WEATHER_STALE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Weather stale", buf);
}

/* 6. Connected + synced + WEATHER_UNAVAILABLE → "Weather N/A" */
static void test_connected_synced_unavailable_shows_weather_na(void)
{
    app_state_t s = make_state(WIFI_STATE_CONNECTED, TIME_STATE_SYNCED, WEATHER_UNAVAILABLE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Weather N/A", buf);
}

/* 7. WIFI_STATE_OFFLINE + time synced → "Wi-Fi: offline" */
static void test_offline_synced_shows_wifi_offline(void)
{
    app_state_t s = make_state(WIFI_STATE_OFFLINE, TIME_STATE_SYNCED, WEATHER_UNAVAILABLE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Wi-Fi: offline", buf);
}

/* 8. WIFI_STATE_OFFLINE + time not synced → "Offline, no sync" */
static void test_offline_not_synced_shows_offline_no_sync(void)
{
    app_state_t s = make_state(WIFI_STATE_OFFLINE, TIME_STATE_NOT_SYNCED, WEATHER_UNAVAILABLE);
    char buf[64];
    build_status_string(&s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Offline, no sync", buf);
}

void run_status_builder_tests(void)
{
    RUN_TEST(test_wifi_init_shows_connecting);
    RUN_TEST(test_wifi_connecting_shows_connecting);
    RUN_TEST(test_connected_no_time_shows_syncing);
    RUN_TEST(test_connected_synced_fresh_shows_synced);
    RUN_TEST(test_connected_synced_stale_shows_weather_stale);
    RUN_TEST(test_connected_synced_unavailable_shows_weather_na);
    RUN_TEST(test_offline_synced_shows_wifi_offline);
    RUN_TEST(test_offline_not_synced_shows_offline_no_sync);
}
