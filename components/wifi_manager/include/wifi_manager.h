#pragma once

#include "app_config.h"
#include "app_state.h"   /* wifi_state_t is defined in app_state.h */

#ifdef ESP_PLATFORM
#  include "freertos/FreeRTOS.h"
#  include "freertos/event_groups.h"
#endif

/* Event bits — exported so time_sync and weather_service can wait on them */
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1

esp_err_t          wifi_manager_init(const device_config_t *config);
void               wifi_manager_start(void);
wifi_state_t       wifi_manager_get_state(void);
EventGroupHandle_t wifi_manager_get_event_group(void);
