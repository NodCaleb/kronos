/*
 * secrets.h — placeholder credentials for the test_time_sync integration test.
 * Fake values only; safe to commit. app_config.c includes secrets.h
 * unconditionally on ESP_PLATFORM builds (first-boot NVS defaults).
 */
#pragma once

#define CONFIG_WIFI_SSID        "test_wifi_ssid"
#define CONFIG_WIFI_PASSWORD    "test_wifi_password"
#define CONFIG_OWM_API_KEY      "test_owm_api_key"
#define CONFIG_WEATHER_LOCATION "lat=0&lon=0"
#define CONFIG_TZ_POSIX         "UTC0"
