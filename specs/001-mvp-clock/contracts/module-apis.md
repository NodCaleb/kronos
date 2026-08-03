# Contract: Module Public APIs

**Branch**: `001-mvp-clock` | **Date**: 2026-08-03

This document specifies the public C interface (`.h` contract) for each firmware module.
All symbols use the `module_name_` prefix (ESP-IDF naming convention).

---

## `wifi_manager`

```c
// components/wifi_manager/include/wifi_manager.h

esp_err_t wifi_manager_init(const device_config_t *config);
// Initialize Wi-Fi stack and start connection attempt.
// Returns: ESP_OK on success; ESP_ERR_* on init failure.

void wifi_manager_start(void);
// Spawn wifi_task. Call once after wifi_manager_init().

wifi_state_t wifi_manager_get_state(void);
// Thread-safe read of current Wi-Fi state.

EventGroupHandle_t wifi_manager_get_event_group(void);
// Returns the FreeRTOS EventGroup used for WIFI_CONNECTED_BIT / WIFI_DISCONNECTED_BIT.
// Other tasks wait on this group to synchronise with Wi-Fi transitions.
```

**Event bits exported**:
```c
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1
```

---

## `time_sync`

```c
// components/time_sync/include/time_sync.h

esp_err_t time_sync_init(const device_config_t *config);
// Configure SNTP and POSIX TZ string. Call after app_config_load().

void time_sync_start(void);
// Spawn ntp_task. Task waits for WIFI_CONNECTED_BIT before first sync.

time_sync_state_t time_sync_get_state(void);
// Returns TIME_STATE_SYNCED or TIME_STATE_NOT_SYNCED.

bool time_sync_get_local_time(struct tm *out_tm);
// Fills *out_tm with current local time. Returns true if synchronized.
// Safe to call from any task; no mutex required (reads system clock).

// host-testable (no ESP32 deps):
int time_format_hms(const struct tm *t, char *buf, size_t len);
// Formats *t as "HH:MM:SS" into buf. Returns chars written (excl. NUL).

int time_format_date(const struct tm *t, char *buf, size_t len);
// Formats *t as "Mon DD Mon" (e.g. "Sun 03 Aug") into buf.
```

---

## `weather_service`

```c
// components/weather_service/include/weather_service.h

esp_err_t weather_service_init(const device_config_t *config, app_state_t *state);
// Store config reference and shared state pointer.

void weather_service_start(void);
// Spawn weather_task. Task waits for WIFI_CONNECTED_BIT before first fetch.

// host-testable (no ESP32 deps):
esp_err_t weather_parse(const char *json, size_t len, weather_data_t *out);
// Parse a One Call 3.0 JSON response into *out.
// Returns: ESP_OK on success; ESP_ERR_INVALID_RESPONSE on rejection.
// Does NOT modify *out on failure — caller retains prior data.
```

---

## `display`

```c
// components/display/include/display.h

esp_err_t display_init(void);
// Initialize I2C bus and SSD1306 driver. Show splash screen (FR-001).

void display_render(const display_payload_t *payload);
// Render payload to display. Uses partial redraw for time digit area.
// Safe to call from clock_task every second.

void display_show_message(const char *line1, const char *line2);
// Show a two-line status message (used during boot/error states).
```

---

## `app_config`

```c
// components/app_config/include/app_config.h

esp_err_t app_config_load(device_config_t *out_config);
// Load config from NVS; on first boot, write values from secrets.h.
// Returns: ESP_OK or ESP_ERR_NVS_* on storage error.

// host-testable (no ESP32 deps):
esp_err_t app_config_validate(const device_config_t *config);
// Validate all fields per data-model.md rules.
// Returns: ESP_OK or ESP_ERR_INVALID_ARG with ESP_LOGE description.
```

---

## `app_state`

```c
// components/app_state/include/app_state.h

esp_err_t app_state_init(void);
// Create the shared app_state_t instance and its FreeRTOS mutex.

void app_state_lock(void);
// Acquire mutex. Callers MUST call app_state_unlock() after modification.

void app_state_unlock(void);
// Release mutex.

app_state_t *app_state_get(void);
// Return pointer to shared state. MUST only be called while holding the mutex.

bool app_state_try_read(app_state_t *out, TickType_t timeout);
// Copy shared state into *out under mutex with timeout.
// Returns false if mutex not acquired within timeout (clock_task path).
```

---

## `error_handler`

```c
// components/error_handler/include/error_handler.h

void error_handler_fatal(const char *tag, const char *message, esp_err_t err);
// Log ESP_LOGE, delay 1 s, then trigger esp_restart().
// Use only for unrecoverable boot failures (e.g., NVS corrupt, display init fail).

void error_handler_warn(const char *tag, const char *message, esp_err_t err);
// Log ESP_LOGW. No restart. Use for transient errors (fetch fail, parse error).
```
