#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "app_config.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* components/weather_service/include/weather_service.h */

#define WEATHER_CONDITION_TEXT_LEN  48
#define WEATHER_FORECAST_SLOTS      6
#define WEATHER_TIME_LABEL_LEN      8   /* "HH:MM\0" */

typedef enum {
    WEATHER_FRESH,       /* age < 2 × refresh_interval */
    WEATHER_STALE,       /* age >= 2 × refresh_interval */
    WEATHER_UNAVAILABLE  /* never successfully fetched */
} weather_freshness_t;

typedef struct {
    char        time_label[WEATHER_TIME_LABEL_LEN];
    float       temperature;
    int         condition_code;
    char        condition_text[WEATHER_CONDITION_TEXT_LEN];
} hourly_slot_t;

typedef struct {
    float               current_temp;
    int                 current_condition_code;    /* retained for future icon rendering */
    char                current_condition_text[WEATHER_CONDITION_TEXT_LEN];
    hourly_slot_t       forecast[WEATHER_FORECAST_SLOTS];
    uint8_t             forecast_count;            /* 0–6 valid entries */
    int64_t             fetch_timestamp_s;         /* Unix epoch, UTC */
    weather_freshness_t freshness;
} weather_data_t;

/* ── Public API ────────────────────────────────────────────────────────── */

esp_err_t weather_service_init(const device_config_t *config, app_state_t *state);
/* Store config reference and shared state pointer. */

void weather_service_start(void);
/* Spawn weather_task (priority 3, 6 KB stack). Task waits for WIFI_CONNECTED_BIT. */

/* host-testable (no ESP32 deps): */
esp_err_t weather_parse(const char *json, size_t len, weather_data_t *out);
/* Parse an OWM One Call 3.0 JSON response into *out.
 * Returns ESP_OK on success; ESP_ERR_INVALID_RESPONSE on any rejection rule.
 * Does NOT modify *out on failure — caller retains prior data. */

#ifdef __cplusplus
}
#endif
