#pragma once

#include <stddef.h>
#include "app_config.h"
#include "app_state.h"  /* canonical owner: weather_freshness_t, hourly_slot_t, weather_data_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ── Public API ────────────────────────────────────────────────────────── */

esp_err_t weather_service_init(const device_config_t *config, app_state_t *state);
void      weather_service_start(void);

/* host-testable (no ESP32 deps): parse OWM One Call 3.0 JSON into *out.
 * Returns ESP_OK on success; ESP_ERR_INVALID_RESPONSE on any rejection rule.
 * Does NOT modify *out on failure — caller retains prior data. */
esp_err_t weather_parse(const char *json, size_t len, weather_data_t *out);

#ifdef __cplusplus
}
#endif
