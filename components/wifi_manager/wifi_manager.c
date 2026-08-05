#include "wifi_manager.h"
#include "error_handler.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wifi_manager";

static const device_config_t  *s_config;
static EventGroupHandle_t       s_event_group;
static volatile wifi_state_t    s_wifi_state;
static TaskHandle_t             s_wifi_task_handle;

/* Set to true by GOT_IP handler so wifi_task resets back-off on next disconnect */
static volatile bool s_was_connected;

/* ── Event handler ───────────────────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* Initial connection attempt on stack start */
        app_state_lock();
        app_state_get()->wifi_state = WIFI_STATE_CONNECTING;
        app_state_unlock();
        s_wifi_state = WIFI_STATE_CONNECTING;
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_event_group, WIFI_DISCONNECTED_BIT);

        app_state_lock();
        app_state_get()->wifi_state = WIFI_STATE_OFFLINE;
        app_state_unlock();
        s_wifi_state = WIFI_STATE_OFFLINE;

        /* Notify wifi_task to schedule a reconnection attempt */
        if (s_wifi_task_handle) {
            vTaskNotifyGiveFromISR(s_wifi_task_handle, NULL);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        s_was_connected = true;
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_event_group, WIFI_DISCONNECTED_BIT);

        app_state_lock();
        app_state_get()->wifi_state = WIFI_STATE_CONNECTED;
        app_state_unlock();
        s_wifi_state = WIFI_STATE_CONNECTED;
    }
}

/* ── wifi_task: exponential back-off reconnection ────────────────────────── */

static void wifi_task(void *pvArg)
{
    uint32_t delay_s = 1;

    while (1) {
        /* Block until the event handler signals a disconnect */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Reset back-off if the previous cycle reached a successful connection */
        if (s_was_connected) {
            s_was_connected = false;
            delay_s = 1;
        }

        ESP_LOGI(TAG, "Disconnected — retrying in %" PRIu32 " s", delay_s);
        vTaskDelay(pdMS_TO_TICKS((uint32_t)(delay_s * 1000)));

        /* Transition to "connecting" before the attempt */
        app_state_lock();
        app_state_get()->wifi_state = WIFI_STATE_CONNECTING;
        app_state_unlock();
        s_wifi_state = WIFI_STATE_CONNECTING;

        esp_wifi_connect();

        /* Double delay, cap at configured maximum */
        uint32_t cap = s_config->wifi_max_retry_interval_s;
        delay_s = (delay_s <= cap / 2) ? delay_s * 2 : cap;
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t wifi_manager_init(const device_config_t *config)
{
    s_config      = config;
    s_wifi_state  = WIFI_STATE_INIT;
    s_was_connected = false;

    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        return ESP_ERR_NO_MEM;
    }
    /* Start with the disconnected bit set */
    xEventGroupSetBits(s_event_group, WIFI_DISCONNECTED_BIT);

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) return err;

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) return err;

    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    strncpy((char *)wifi_cfg.sta.ssid,     config->wifi_ssid,
            sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, config->wifi_password,
            sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) return err;

    err = esp_wifi_start(); /* fires WIFI_EVENT_STA_START → first connect */
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Wi-Fi init OK, SSID: %s", config->wifi_ssid);
    return ESP_OK;
}

void wifi_manager_start(void)
{
    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 4, &s_wifi_task_handle);
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

EventGroupHandle_t wifi_manager_get_event_group(void)
{
    return s_event_group;
}
