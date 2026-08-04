#pragma once

#include <stdint.h>

/* ── Host/target portability ─────────────────────────────────────────────── */
#ifndef ESP_PLATFORM
#  ifndef KRONOS_HOST_ESP_ERR
#  define KRONOS_HOST_ESP_ERR
   typedef int esp_err_t;
#  define ESP_OK              0
#  define ESP_ERR_INVALID_ARG (-1)
#  endif
#else
#  include "esp_err.h"
#endif

/* ── Field size constants ────────────────────────────────────────────────── */
#define CONFIG_WIFI_SSID_LEN        33
#define CONFIG_WIFI_PASSWORD_LEN    65
#define CONFIG_NTP_SERVER_LEN       64
#define CONFIG_TZ_STRING_LEN        48
#define CONFIG_WEATHER_URL_LEN     128
#define CONFIG_WEATHER_API_KEY_LEN  64
#define CONFIG_WEATHER_LOCATION_LEN 32

/* ── Types ───────────────────────────────────────────────────────────────── */
typedef enum {
    UNITS_METRIC,
    UNITS_IMPERIAL
} measurement_units_t;

typedef struct {
    /* Network */
    char                wifi_ssid[CONFIG_WIFI_SSID_LEN];
    char                wifi_password[CONFIG_WIFI_PASSWORD_LEN];
    uint32_t            wifi_max_retry_interval_s; /* exponential back-off cap */

    /* Time */
    char                ntp_server[CONFIG_NTP_SERVER_LEN];
    char                tz_posix[CONFIG_TZ_STRING_LEN];

    /* Weather */
    char                weather_api_url[CONFIG_WEATHER_URL_LEN];
    char                weather_api_key[CONFIG_WEATHER_API_KEY_LEN];
    char                weather_location[CONFIG_WEATHER_LOCATION_LEN];
    measurement_units_t units;
    uint32_t            weather_refresh_interval_s;
    uint32_t            request_timeout_ms;

    /* Diagnostics */
    uint8_t             log_level; /* maps to esp_log_level_t */
} device_config_t;

/* ── Public API ──────────────────────────────────────────────────────────── */
esp_err_t app_config_load(device_config_t *out_config);
esp_err_t app_config_validate(const device_config_t *config);
