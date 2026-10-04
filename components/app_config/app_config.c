#include "app_config.h"
#include <string.h>
#include <stdbool.h>

/* ── Platform portability ────────────────────────────────────────────────── */
#ifdef ESP_PLATFORM
#  include "esp_log.h"
#  include "nvs_flash.h"
#  include "nvs.h"
#  include "secrets.h"
#else
#  define ESP_LOGE(tag, fmt, ...) ((void)0)
#  define ESP_LOGW(tag, fmt, ...) ((void)0)
#  define ESP_LOGI(tag, fmt, ...) ((void)0)
#  define ESP_ERROR_CHECK(x)      ((void)(x))
#endif

#define NVS_NAMESPACE "kronos_cfg"

static const char *TAG = "app_config";

/* ── Validation (host-testable — no ESP32 platform deps) ─────────────────── */
esp_err_t app_config_validate(const device_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->wifi_ssid[0] == '\0') {
        ESP_LOGE(TAG, "wifi_ssid must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (strncmp(config->weather_api_url, "https://", 8) != 0) {
        ESP_LOGE(TAG, "weather_api_url must start with https://");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->weather_api_key[0] == '\0') {
        ESP_LOGE(TAG, "weather_api_key must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->ntp_server[0] == '\0') {
        ESP_LOGE(TAG, "ntp_server must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->tz_posix[0] == '\0') {
        ESP_LOGE(TAG, "tz_posix must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->weather_refresh_interval_s < 60 ||
        config->weather_refresh_interval_s > 3600) {
        ESP_LOGE(TAG, "weather_refresh_interval_s must be in [60, 3600]");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->request_timeout_ms < 1000 ||
        config->request_timeout_ms > 30000) {
        ESP_LOGE(TAG, "request_timeout_ms must be in [1000, 30000]");
        return ESP_ERR_INVALID_ARG;
    }
    /* SC-004: max retry cap <= 300 s enforces 5-minute resume SLA */
    if (config->wifi_max_retry_interval_s < 1 ||
        config->wifi_max_retry_interval_s > 300) {
        ESP_LOGE(TAG, "wifi_max_retry_interval_s must be in [1, 300]");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/* ── NVS load (ESP32-only) ───────────────────────────────────────────────── */
#ifdef ESP_PLATFORM

/* FNV-1a over secrets.h values; a changed hash means secrets.h was edited since the last flash */
static uint32_t compute_secrets_hash(void)
{
    const char *fields[] = {
        CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, CONFIG_OWM_API_KEY,
        CONFIG_WEATHER_LOCATION, CONFIG_TZ_POSIX,
    };
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        for (const char *p = fields[i]; *p != '\0'; p++) {
            hash ^= (uint8_t)*p;
            hash *= 16777619u;
        }
        hash ^= 0xFFu; /* separator so field boundaries affect the hash */
    }
    return hash;
}

esp_err_t app_config_load(device_config_t *out_config)
{
    if (!out_config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS truncated or version changed — erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_handle_t h;
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        return ret;
    }

    uint32_t current_hash = compute_secrets_hash();
    uint32_t stored_hash   = 0;
    bool secrets_changed = (nvs_get_u32(h, "cfg_hash", &stored_hash) != ESP_OK) ||
                           (stored_hash != current_hash);

    if (secrets_changed) {
        /* First boot, or secrets.h was edited since the last flash: (re)write defaults */
        ESP_LOGI(TAG, "secrets.h changed (or first boot) — writing config to NVS");
        nvs_set_str(h, "wifi_ssid",   CONFIG_WIFI_SSID);
        nvs_set_str(h, "wifi_pass",   CONFIG_WIFI_PASSWORD);
        nvs_set_str(h, "ntp_srv",     "pool.ntp.org");
        nvs_set_str(h, "tz_posix",    CONFIG_TZ_POSIX);
        nvs_set_str(h, "api_url",
                    "https://api.openweathermap.org/data/3.0/onecall");
        nvs_set_str(h, "api_key",     CONFIG_OWM_API_KEY);
        nvs_set_str(h, "location",    CONFIG_WEATHER_LOCATION);
        nvs_set_u32(h, "refresh_s",   900);
        nvs_set_u32(h, "timeout_ms",  10000);
        nvs_set_u32(h, "retry_cap_s", 300);
        nvs_set_u8(h,  "units",       (uint8_t)UNITS_METRIC);
        nvs_set_u8(h,  "log_level",   3); /* ESP_LOG_INFO */
        nvs_set_u32(h, "cfg_hash",    current_hash);
        ret = nvs_commit(h);
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
    }

    /* Read all config values from NVS */
    size_t len;

    len = sizeof(out_config->wifi_ssid);
    ret = nvs_get_str(h, "wifi_ssid", out_config->wifi_ssid, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    len = sizeof(out_config->wifi_password);
    ret = nvs_get_str(h, "wifi_pass", out_config->wifi_password, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    len = sizeof(out_config->ntp_server);
    ret = nvs_get_str(h, "ntp_srv", out_config->ntp_server, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    len = sizeof(out_config->tz_posix);
    ret = nvs_get_str(h, "tz_posix", out_config->tz_posix, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    len = sizeof(out_config->weather_api_url);
    ret = nvs_get_str(h, "api_url", out_config->weather_api_url, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    len = sizeof(out_config->weather_api_key);
    ret = nvs_get_str(h, "api_key", out_config->weather_api_key, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    len = sizeof(out_config->weather_location);
    ret = nvs_get_str(h, "location", out_config->weather_location, &len);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    ret = nvs_get_u32(h, "refresh_s", &out_config->weather_refresh_interval_s);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    ret = nvs_get_u32(h, "timeout_ms", &out_config->request_timeout_ms);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    ret = nvs_get_u32(h, "retry_cap_s", &out_config->wifi_max_retry_interval_s);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    uint8_t units_val = 0;
    ret = nvs_get_u8(h, "units", &units_val);
    if (ret != ESP_OK) { nvs_close(h); return ret; }
    out_config->units = (measurement_units_t)units_val;

    ret = nvs_get_u8(h, "log_level", &out_config->log_level);
    if (ret != ESP_OK) { nvs_close(h); return ret; }

    nvs_close(h);

    ret = app_config_validate(out_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Config loaded from NVS (ssid=\"%s\")", out_config->wifi_ssid);
    return ESP_OK;
}

#endif /* ESP_PLATFORM */
