#include "app_state.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_state";

static app_state_t      s_state;
static SemaphoreHandle_t s_mutex;

esp_err_t app_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.wifi_state       = WIFI_STATE_INIT;
    s_state.time_state       = TIME_STATE_NOT_SYNCED;
    s_state.weather.freshness = WEATHER_UNAVAILABLE;

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void app_state_lock(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
}

void app_state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

app_state_t *app_state_get(void)
{
    return &s_state;
}

/* Copy state into *out under mutex; returns false if timeout exceeded */
bool app_state_try_read(app_state_t *out, TickType_t timeout)
{
    if (xSemaphoreTake(s_mutex, timeout) != pdTRUE) {
        return false;
    }
    memcpy(out, &s_state, sizeof(app_state_t));
    xSemaphoreGive(s_mutex);
    return true;
}
