#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ── Host/target portability ─────────────────────────────────────────────── */
#ifndef ESP_PLATFORM
#  ifndef KRONOS_HOST_ESP_ERR
#  define KRONOS_HOST_ESP_ERR
   typedef int esp_err_t;
#  define ESP_OK              0
#  define ESP_ERR_INVALID_ARG (-1)
#  define ESP_ERR_NO_MEM      (-2)
#  endif
#  ifndef KRONOS_HOST_FREERTOS
#  define KRONOS_HOST_FREERTOS
   typedef unsigned int TickType_t;
#  define portMAX_DELAY       ((TickType_t)0xffffffffU)
#  endif
#else
#  include "esp_err.h"
#  include "freertos/FreeRTOS.h"
#endif

/* ── Weather types (canonical here; weather_service.h includes this file) ── */

#define WEATHER_CONDITION_TEXT_LEN  48
#define WEATHER_FORECAST_SLOTS       6
#define WEATHER_TIME_LABEL_LEN       8  /* "HH:MM\0" + pad */

typedef enum {
    WEATHER_FRESH,
    WEATHER_STALE,
    WEATHER_UNAVAILABLE
} weather_freshness_t;

typedef struct {
    char  time_label[WEATHER_TIME_LABEL_LEN];
    float temperature;
    int   condition_code;
    char  condition_text[WEATHER_CONDITION_TEXT_LEN];
} hourly_slot_t;

typedef struct {
    float               current_temp;
    int                 current_condition_code;  /* retained for future icon rendering */
    char                current_condition_text[WEATHER_CONDITION_TEXT_LEN];
    hourly_slot_t       forecast[WEATHER_FORECAST_SLOTS];
    uint8_t             forecast_count;          /* 0–6 valid entries */
    int64_t             fetch_timestamp_s;       /* Unix epoch, UTC */
    weather_freshness_t freshness;
} weather_data_t;

/* ── Device state enums ──────────────────────────────────────────────────── */

typedef enum {
    WIFI_STATE_INIT,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_OFFLINE
} wifi_state_t;

/* Canonical definition — time_sync.h includes this file for this type */
typedef enum {
    TIME_STATE_NOT_SYNCED,
    TIME_STATE_SYNCED
} time_sync_state_t;

/* ── Shared runtime state ────────────────────────────────────────────────── */

typedef struct {
    wifi_state_t       wifi_state;
    time_sync_state_t  time_state;
    struct tm          local_time;          /* updated every second by clock_task */
    weather_data_t     weather;             /* last valid snapshot */
    int64_t            last_weather_update_s;
    char               status_message[64]; /* human-readable footer string */
} app_state_t;

/* ── Accessor API ────────────────────────────────────────────────────────── */

esp_err_t    app_state_init(void);
void         app_state_lock(void);
void         app_state_unlock(void);
app_state_t *app_state_get(void);
bool         app_state_try_read(app_state_t *out, TickType_t timeout);
