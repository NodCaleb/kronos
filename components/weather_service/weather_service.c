/* weather_service.c — periodic HTTPS weather fetch from OWM One Call 3.0 */

#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "wifi_manager.h"
#include "app_state.h"
#include "app_config.h"
#include "weather_service.h"

static const char *TAG = "weather_service";

/* ── Module state ─────────────────────────────────────────────────────── */

#define WEATHER_RESP_BUF_SIZE   8192
#define URL_BUF_SIZE            256

static const device_config_t *s_config = NULL;
static app_state_t            *s_state  = NULL;

/* ── Response accumulation buffer (must be static — too large for the task stack) ─ */

typedef struct {
    char  buf[WEATHER_RESP_BUF_SIZE];
    int   len;
    bool  overflow;
} resp_buf_t;

/* ── esp_http_client event handler ───────────────────────────────────── */

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    resp_buf_t *rb = (resp_buf_t *)evt->user_data;
    if (rb == NULL) {
        return ESP_OK;
    }
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!rb->overflow) {
                int remaining = (int)sizeof(rb->buf) - rb->len - 1;
                if (evt->data_len > remaining) {
                    rb->overflow = true;
                    ESP_LOGW(TAG, "Response buffer overflow — response truncated");
                } else {
                    memcpy(rb->buf + rb->len, evt->data, evt->data_len);
                    rb->len += evt->data_len;
                    rb->buf[rb->len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
        case HTTP_EVENT_DISCONNECTED:
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* ── Build the request URL ────────────────────────────────────────────── */

static void build_url(const device_config_t *cfg, char *out, size_t out_len)
{
    /* weather_location holds "lat,lon" e.g. "48.8566,2.3522" */
    char lat_buf[24] = {0};
    char lon_buf[24] = {0};
    const char *loc = cfg->weather_location;
    const char *comma = strchr(loc, ',');
    if (comma != NULL) {
        size_t lat_len = (size_t)(comma - loc);
        if (lat_len >= sizeof(lat_buf)) lat_len = sizeof(lat_buf) - 1;
        strncpy(lat_buf, loc, lat_len);
        lat_buf[lat_len] = '\0';
        strncpy(lon_buf, comma + 1, sizeof(lon_buf) - 1);
        lon_buf[sizeof(lon_buf) - 1] = '\0';
    }

    const char *units_str = (cfg->units == UNITS_IMPERIAL) ? "imperial" : "metric";
    snprintf(out, out_len,
             "%s?lat=%s&lon=%s&exclude=minutely,daily,alerts&units=%s&appid=%s",
             cfg->weather_api_url, lat_buf, lon_buf, units_str, cfg->weather_api_key);
}

/* ── weather_task ─────────────────────────────────────────────────────── */

static void weather_task(void *pvArg)
{
    (void)pvArg;

    EventGroupHandle_t event_group = wifi_manager_get_event_group();
    char url[URL_BUF_SIZE];
    build_url(s_config, url, sizeof(url));
    /* Debug only — includes the API key in cleartext; remove/guard before shipping */
    ESP_LOGI(TAG, "Request URL: %s", url);

    uint32_t retry_interval_s = s_config->weather_refresh_interval_s;

    while (1) {
        /* Wait for Wi-Fi before attempting fetch */
        xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT,
                            pdFALSE, pdTRUE, portMAX_DELAY);

        /* ── Staleness check ─────────────────────────────────────────── */
        {
            /* app_state_lock also serializes libc time() across tasks (avoids concurrent-first-call kernel lock corruption) */
            app_state_lock();
            int64_t now_s = (int64_t)time(NULL);
            int64_t stale_threshold_s = (int64_t)(2 * s_config->weather_refresh_interval_s);
            app_state_t *st = app_state_get();
            if (st->weather.freshness != WEATHER_UNAVAILABLE &&
                st->weather.fetch_timestamp_s > 0 &&
                (now_s - st->weather.fetch_timestamp_s) >= stale_threshold_s) {
                st->weather.freshness = WEATHER_STALE;
            }
            app_state_unlock();
        }

        ESP_LOGI(TAG, "Fetching weather from OWM...");

        /* ── HTTP request ────────────────────────────────────────────── */
        /* static: 8 KB response buffer must not live on weather_task's stack */
        static resp_buf_t rb;
        memset(&rb, 0, sizeof(rb));

        esp_http_client_config_t http_cfg = {
            .url              = url,
            .method           = HTTP_METHOD_GET,
            .timeout_ms       = (int)s_config->request_timeout_ms,
            .user_agent       = "Kronos/1.0 ESP32",
            .crt_bundle_attach = esp_crt_bundle_attach,
            .event_handler    = http_event_handler,
            .user_data        = &rb,
            .disable_auto_redirect = false,
        };

        esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
        if (client == NULL) {
            ESP_LOGW(TAG, "esp_http_client_init failed; will retry");
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        esp_err_t err = esp_http_client_perform(client);
        int http_status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_ERR_HTTP_CONNECT || err == ESP_ERR_HTTP_FETCH_HEADER) {
            ESP_LOGW(TAG, "Request timed out (err=0x%x)", err);
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        if (err == ESP_ERR_HTTP_CONNECT) {
            ESP_LOGE(TAG, "TLS error (err=0x%x)", err);
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Request timed out (err=0x%x)", err);
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        /* ── Handle HTTP status ──────────────────────────────────────── */
        if (http_status == 401) {
            ESP_LOGE(TAG, "API key invalid (HTTP 401); marking weather UNAVAILABLE");
            app_state_lock();
            app_state_get()->weather.freshness = WEATHER_UNAVAILABLE;
            app_state_unlock();
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        if (http_status == 429) {
            uint32_t backoff_s = retry_interval_s * 2;
            if (backoff_s > 3600) backoff_s = 3600;
            ESP_LOGW(TAG, "Rate limited (HTTP 429); backing off %"PRIu32" s", backoff_s);
            retry_interval_s = backoff_s;
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        if (http_status >= 500 && http_status < 600) {
            ESP_LOGW(TAG, "Server error %d; retaining stale data", http_status);
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        if (http_status != 200) {
            ESP_LOGW(TAG, "Unexpected HTTP status %d; retaining stale data", http_status);
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        /* Reset back-off on successful HTTP 200 */
        retry_interval_s = s_config->weather_refresh_interval_s;

        if (rb.overflow) {
            ESP_LOGE(TAG, "JSON parse error: response too large for buffer");
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        /* ── Parse response ──────────────────────────────────────────── */
        weather_data_t parsed;
        app_state_lock();
        /* Copy current weather as base so parse failure leaves it intact */
        parsed = app_state_get()->weather;
        app_state_unlock();

        esp_err_t parse_ret = weather_parse(rb.buf, (size_t)rb.len, &parsed);
        if (parse_ret != ESP_OK) {
            ESP_LOGE(TAG, "JSON parse error: weather_parse returned 0x%x", parse_ret);
            vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
            continue;
        }

        /* ── Update shared state ─────────────────────────────────────── */
        app_state_lock();
        app_state_t *st = app_state_get();
        st->weather                = parsed;
        st->weather.freshness      = WEATHER_FRESH;
        st->last_weather_update_s  = parsed.fetch_timestamp_s;
        app_state_unlock();

        ESP_LOGI(TAG, "Weather updated: %.1f°  %s  (%d forecast slots)",
                 (double)parsed.current_temp,
                 parsed.current_condition_text,
                 parsed.forecast_count);

        vTaskDelay(pdMS_TO_TICKS(retry_interval_s * 1000));
    }
}

/* ── Public API ───────────────────────────────────────────────────────── */

esp_err_t weather_service_init(const device_config_t *config, app_state_t *state)
{
    if (config == NULL || state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = config;
    s_state  = state;
    ESP_LOGI(TAG, "weather_service_init: OK (url=%s)", config->weather_api_url);
    return ESP_OK;
}

void weather_service_start(void)
{
    /* Pinned to core 0 to rule out SMP cross-core races */
    xTaskCreatePinnedToCore(weather_task, "weather_task", 8192, NULL, 3, NULL, 0);
}
