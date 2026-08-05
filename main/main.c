/* Init order: app_config → app_state → display → wifi_manager → time_sync
 *             → weather_service → clock_task
 */
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "app_state.h"
#include "display.h"
#include "time_sync.h"
#include "error_handler.h"
#include "wifi_manager.h"
#include "weather_service.h"

static const char *TAG = "main";

/* clock_task — priority 5, 4 KB stack; drives display every second */
static void clock_task(void *pvArg)
{
    (void)pvArg;
    app_state_t       snap;
    display_payload_t payload;

    memset(&payload, 0, sizeof(payload));
    strncpy(payload.weather_summary, "Weather N/A", sizeof(payload.weather_summary) - 1);
    strncpy(payload.status, "Time: syncing", sizeof(payload.status) - 1);

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
            payload.weather_stale = (snap.weather.freshness == WEATHER_STALE);
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

            /* Status string */
            if (snap.time_state == TIME_STATE_SYNCED) {
                strncpy(payload.status, "Synced", sizeof(payload.status) - 1);
            } else {
                strncpy(payload.status, "Time: syncing", sizeof(payload.status) - 1);
            }
        }

        display_render(&payload);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    device_config_t config;
    esp_err_t ret = app_config_load(&config);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "app_config_load failed", ret);
    }

    ret = app_state_init();
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "app_state_init failed", ret);
    }

    /* display_init shows splash screen "Kronos v0.1.0" within 2 s (SC-001) */
    ret = display_init();
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "display_init failed", ret);
    }

    ret = wifi_manager_init(&config);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "wifi_manager_init failed", ret);
    }
    wifi_manager_start(); /* spawns wifi_task (priority 4) */

    ret = time_sync_init(&config);
    if (ret != ESP_OK) {
        error_handler_fatal(TAG, "time_sync_init failed", ret);
    }
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
