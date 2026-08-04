/* Init order: app_config → app_state → display → [wifi Phase 4] → [ntp Phase 5]
 *             → [weather Phase 6] → clock_task
 */
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "app_state.h"
#include "display.h"
#include "time_sync.h"
#include "error_handler.h"

static const char *TAG = "main";

/* clock_task — priority 5, 4 KB stack; drives display every second */
static void clock_task(void *pvArg)
{
    (void)pvArg;
    app_state_t       snap;
    display_payload_t payload;

    memset(&payload, 0, sizeof(payload));
    strncpy(payload.weather_summary, "Weather N/A", sizeof(payload.weather_summary) - 1);
    strncpy(payload.status, "OK", sizeof(payload.status) - 1);

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
            payload.weather_stale = (snap.weather.freshness == WEATHER_STALE);
        }

        display_render(&payload);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    /* Phase 3 init sequence (wifi / ntp / weather added in later phases) */
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

    xTaskCreate(clock_task, "clock_task", 4096, NULL, 5, NULL);
}
