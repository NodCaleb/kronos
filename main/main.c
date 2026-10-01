#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_config.h"
#include "app_state.h"
#include "display.h"
#include "time_sync.h"
#include "error_handler.h"
#include "wifi_manager.h"
#include "weather_service.h"
#include "status_builder.h"

static const char *TAG = "main";

/* clock_task — priority 5, 4 KB stack; drives display every second */
static void clock_task(void *pvArg)
{
    (void)pvArg;
    app_state_t       snap;
    display_payload_t payload;

    memset(&snap,    0, sizeof(snap));
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.weather_summary, "Weather N/A", sizeof(payload.weather_summary) - 1);

    while (1) {
        /* Keep app_state.local_time current from system clock */
        time_t     now = time(NULL);
        struct tm  tm_local;
        localtime_r(&now, &tm_local);

        app_state_lock();
        app_state_get()->local_time = tm_local;
        app_state_unlock();

        /* Snapshot shared state (5 ms timeout — never stalls display) */
        if (app_state_try_read(&snap, pdMS_TO_TICKS(5))) {
            time_format_hms(&snap.local_time,  payload.time_str,  sizeof(payload.time_str));
            time_format_date(&snap.local_time, payload.date_str,  sizeof(payload.date_str));

            /* Weather summary */
            if (snap.weather.freshness == WEATHER_UNAVAILABLE) {
                strncpy(payload.weather_summary, "Weather N/A",
                        sizeof(payload.weather_summary) - 1);
                payload.weather_summary[sizeof(payload.weather_summary) - 1] = '\0';
            } else {
                snprintf(payload.weather_summary, sizeof(payload.weather_summary),
                         "%.0f\xC2\xB0  %s",
                         (double)snap.weather.current_temp,
                         snap.weather.current_condition_text);
            }

            /* Forecast lines (up to DISPLAY_FORECAST_LINES slots) */
            int slots = snap.weather.forecast_count < DISPLAY_FORECAST_LINES
                        ? snap.weather.forecast_count : DISPLAY_FORECAST_LINES;
            for (int i = 0; i < slots; i++) {
                snprintf(payload.forecast[i], sizeof(payload.forecast[i]),
                         "%s  %.0f\xC2\xB0  %s",
                         snap.weather.forecast[i].time_label,
                         (double)snap.weather.forecast[i].temperature,
                         snap.weather.forecast[i].condition_text);
            }
            for (int i = slots; i < DISPLAY_FORECAST_LINES; i++) {
                payload.forecast[i][0] = '\0';
            }

        }

        /* Always reflect latest snapshot — executes even when mutex times out */
        payload.weather_stale = (snap.weather.freshness == WEATHER_STALE);
        build_status_string(&snap, payload.status, sizeof(payload.status));

        display_render(&payload);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    /* ── Module wiring order (Principle VI — main.c is sole wiring point) ──────
     * 1. app_state_init()          [app_state]    — mutex + shared state struct
     * 2. display_init()            [display]      — I2C + OLED + splash screen
     * 3. app_config_load()         [app_config]   — NVS → device_config_t
     * 4. wifi_manager_init/start() [wifi_manager] — station mode + event group
     * 5. time_sync_init/start()    [time_sync]    — SNTP + ntp_task (waits WIFI_CONNECTED_BIT)
     * 6. weather_service_init/start() [weather_service] — HTTPS fetch + weather_task
     * 7. xTaskCreate(clock_task)   [main]         — drives display_render() every second
     * ─────────────────────────────────────────────────────────────────────── */
    ESP_LOGI("KRONOS", "Booting Kronos v0.1.0");

    esp_err_t ret = app_state_init();
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "app_state_init failed", ret);
    }

    /* display_init shows splash screen "Kronos v0.1.0" within 2 s (SC-001, Principle XI) */
    ret = display_init();
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "display_init failed", ret);
    }

    display_show_message("Loading config...", NULL);
    device_config_t config;
    ret = app_config_load(&config);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "app_config_load failed", ret);
    }

    ret = wifi_manager_init(&config);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "wifi_manager_init failed", ret);
    }
    display_show_message("Connecting Wi-Fi...", NULL);
    wifi_manager_start(); /* spawns wifi_task (priority 4) */

    ret = time_sync_init(&config);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "time_sync_init failed", ret);
    }
    display_show_message("Syncing time...", NULL);
    time_sync_start(); /* spawns ntp_task (priority 3) */

    /* Pass shared state pointer to weather_service (pointer is stable after app_state_init) */
    app_state_lock();
    app_state_t *shared_state = app_state_get();
    app_state_unlock();
    ret = weather_service_init(&config, shared_state);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "weather_service_init failed", ret);
    }
    weather_service_start(); /* spawns weather_task (priority 3) */

    xTaskCreate(clock_task, "clock_task", 4096, NULL, 5, NULL);
}
