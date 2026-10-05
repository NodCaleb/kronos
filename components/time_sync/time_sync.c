#include "time_sync.h"
#include "wifi_manager.h"
#include "app_state.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <time.h>
#include <stdlib.h>

/* NTP sync parameters */
#define NTP_SYNC_POLL_INTERVAL_MS  2000U
#define NTP_SYNC_TIMEOUT_COUNT     15        /* 15 × 2 s = 30 s max */
#define NTP_RESYNC_INTERVAL_MS     (3UL * 3600UL * 1000UL)
#define NTP_RETRY_INTERVAL_MS      (60UL * 1000UL)

static const char *TAG = "time_sync";

static const device_config_t     *s_config;
static EventGroupHandle_t         s_event_group;
static volatile time_sync_state_t s_sync_state = TIME_STATE_NOT_SYNCED;

/* ── NTP task ─────────────────────────────────────────────────────────── */

static void ntp_task(void *pvArg)
{
    (void)pvArg;

    while (1) {
        /* Block until Wi-Fi is available */
        xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT,
                            pdFALSE, pdFALSE, portMAX_DELAY);

        ESP_LOGI(TAG, "Wi-Fi connected — initiating SNTP sync");

        TickType_t ticks_start = xTaskGetTickCount();
        /* app_state_lock also serializes libc time() across tasks (avoids concurrent-first-call kernel lock corruption) */
        app_state_lock();
        time_t t_before = time(NULL);
        app_state_unlock();

        esp_sntp_init();

        /* Poll until sync completes or 30 s timeout */
        bool synced = false;
        for (int i = 0; i < NTP_SYNC_TIMEOUT_COUNT; i++) {
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                synced = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_POLL_INTERVAL_MS));
        }

        esp_sntp_stop();

        if (synced) {
            TickType_t ticks_now = xTaskGetTickCount();
            app_state_lock();
            time_t t_after = time(NULL);
            app_state_unlock();

            /* On the first-ever sync t_before is still ~epoch, so a before/after delta
             * isn't a meaningful drift figure — only compute it on subsequent re-syncs. */
            if (s_sync_state == TIME_STATE_SYNCED) {
                float elapsed_s = (float)(ticks_now - ticks_start) *
                                  (float)portTICK_PERIOD_MS / 1000.0f;
                float drift_s   = (float)(t_after - t_before) - elapsed_s;
                ESP_LOGI(TAG, "Time synchronized. Drift: %.2f s", drift_s);
            } else {
                ESP_LOGI(TAG, "Time synchronized.");
            }

            app_state_lock();
            app_state_get()->time_state = TIME_STATE_SYNCED;
            app_state_unlock();
            s_sync_state = TIME_STATE_SYNCED;

            /* Re-sync every 3 hours */
            vTaskDelay(pdMS_TO_TICKS(NTP_RESYNC_INTERVAL_MS));
        } else {
            ESP_LOGW(TAG, "NTP sync timed out — retrying in 60 s");
            vTaskDelay(pdMS_TO_TICKS(NTP_RETRY_INTERVAL_MS));
        }
    }
}

/* ── Public API ───────────────────────────────────────────────────────── */

/* T028 — configure SNTP and POSIX TZ; store Wi-Fi event group */
esp_err_t time_sync_init(const device_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config      = config;
    s_event_group = wifi_manager_get_event_group();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, config->ntp_server);

    setenv("TZ", config->tz_posix, 1);
    tzset();

    ESP_LOGI(TAG, "Initialized: server=%s tz=%s", config->ntp_server, config->tz_posix);
    return ESP_OK;
}

/* T029 — spawn ntp_task (priority 3, 8 KB stack); pinned to core 0 to rule out SMP cross-core races */
void time_sync_start(void)
{
    xTaskCreatePinnedToCore(ntp_task, "ntp_task", 8 * 1024, NULL, 3, NULL, 0);
}

/* T030 — thread-safe state accessor */
time_sync_state_t time_sync_get_state(void)
{
    return s_sync_state;
}

/* T030 — read system clock; returns true only when time is synchronized */
bool time_sync_get_local_time(struct tm *out_tm)
{
    if (out_tm == NULL) {
        return false;
    }
    time_t now = time(NULL);
    localtime_r(&now, out_tm);
    return (s_sync_state == TIME_STATE_SYNCED);
}
