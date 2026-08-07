# Module Reference: Kronos MVP

**Version**: 0.1.0 | **Framework**: ESP-IDF v5.x

Each module is an ESP-IDF component under `components/<name>/`. Every module exposes
exactly one public header at `components/<name>/include/<name>.h`. No `.c`-to-`.c`
cross-component includes exist.

---

## `app_config`

**Directory**: `components/app_config/`  
**Purpose**: Load and validate device configuration. The sole module that reads `secrets.h`
and touches NVS namespace `kronos_cfg`. All other modules receive config via pointer at
init time.

**FreeRTOS task**: None — synchronous, called once during boot.

**Public API**:

```c
esp_err_t app_config_load(device_config_t *out_config);
// Load config from NVS. On first boot, writes values from secrets.h.
// Returns: ESP_OK or ESP_ERR_NVS_* on storage error.

esp_err_t app_config_validate(const device_config_t *config);
// Validate all fields per rules below. Host-testable — no ESP32 platform deps.
// Returns: ESP_OK or ESP_ERR_INVALID_ARG (with ESP_LOGE description).
```

**Validation rules** (`app_config_validate`):
- `wifi_ssid` must be non-empty
- `weather_api_url` must start with `https://`
- `weather_refresh_interval_s`: 60–3600
- `request_timeout_ms`: 1000–30000
- `wifi_max_retry_interval_s`: 1–300

**Usage example**:
```c
device_config_t config;
ESP_ERROR_CHECK(app_config_load(&config));
// config is now ready to pass to other module _init() calls
```

---

## `app_state`

**Directory**: `components/app_state/`  
**Purpose**: Shared runtime state for the device. Single `app_state_t` instance protected
by a FreeRTOS mutex. All inter-task state sharing passes through this module.

**FreeRTOS task**: None — provides mutex-protected accessors used by all tasks.

**Public API**:

```c
esp_err_t app_state_init(void);
// Create the shared app_state_t and its FreeRTOS mutex. Call once at boot.

void app_state_lock(void);
void app_state_unlock(void);
// Acquire/release the mutex. MUST be paired. Used by writer tasks.

app_state_t *app_state_get(void);
// Return pointer to shared state. MUST only be called while holding the mutex.

bool app_state_try_read(app_state_t *out, TickType_t timeout);
// Copy shared state into *out with a mutex timeout.
// Returns false if mutex not acquired within timeout (clock_task uses 5 ms).
```

**Key fields in `app_state_t`**:

| Field | Type | Owner |
|---|---|---|
| `wifi_state` | `wifi_state_t` | `wifi_task` writes |
| `time_state` | `time_sync_state_t` | `ntp_task` writes |
| `local_time` | `struct tm` | `clock_task` writes |
| `weather` | `weather_data_t` | `weather_task` writes |
| `last_weather_update_s` | `int64_t` | `weather_task` writes |
| `status_message[64]` | `char[]` | `clock_task` writes via `build_status_string()` |

**Usage example** (writer task):
```c
app_state_lock();
app_state_t *s = app_state_get();
s->wifi_state = WIFI_STATE_CONNECTED;
app_state_unlock();
```

**Usage example** (reader — `clock_task`):
```c
app_state_t snapshot;
if (app_state_try_read(&snapshot, pdMS_TO_TICKS(5))) {
    // use snapshot fields
}
```

---

## `error_handler`

**Directory**: `components/error_handler/`  
**Purpose**: Uniform error reporting across all modules. Two severity levels: warn (log
only) and fatal (log + 1 s delay + `esp_restart()`).

**FreeRTOS task**: None.

**Public API**:

```c
void error_handler_fatal(const char *tag, const char *message, esp_err_t err);
// Log ESP_LOGE, delay 1 s, call esp_restart().

void error_handler_warn(const char *tag, const char *message, esp_err_t err);
// Log ESP_LOGW. No restart.
```

**Usage example**:
```c
esp_err_t ret = nvs_flash_init();
if (ret != ESP_OK) {
    error_handler_fatal("APP_CONFIG", "NVS init failed", ret);
}
```

---

## `wifi_manager`

**Directory**: `components/wifi_manager/`  
**Purpose**: Wi-Fi station init, connection management, and automatic reconnect with
exponential back-off (1 s → doubles → capped at `config->wifi_max_retry_interval_s`).
Provides an `EventGroupHandle_t` for other tasks to synchronise on Wi-Fi transitions.

**FreeRTOS task**: `wifi_task` — priority 4, 4 KB stack.

**Event bits exported**:
```c
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1
```

**Public API**:

```c
esp_err_t wifi_manager_init(const device_config_t *config);
// Init esp_wifi stack in station mode; register event handlers; create EventGroup.
// Returns: ESP_OK or ESP_ERR_*.

void wifi_manager_start(void);
// Spawn wifi_task. Call once after wifi_manager_init().

wifi_state_t wifi_manager_get_state(void);
// Thread-safe read of current Wi-Fi state.

EventGroupHandle_t wifi_manager_get_event_group(void);
// Returns the EventGroup used for WIFI_CONNECTED_BIT / WIFI_DISCONNECTED_BIT.
```

**Usage example**:
```c
ESP_ERROR_CHECK(wifi_manager_init(&config));
wifi_manager_start();
// pass event group to time_sync and weather_service:
EventGroupHandle_t eg = wifi_manager_get_event_group();
```

---

## `time_sync`

**Directory**: `components/time_sync/`  
**Purpose**: SNTP initialisation, POSIX TZ string application, periodic re-sync every
3 hours, and host-testable time formatting helpers.

**FreeRTOS task**: `ntp_task` — priority 3, 3 KB stack. Waits on `WIFI_CONNECTED_BIT`
before first sync.

**Source files**:
- `time_sync.c` — ESP32 platform code (SNTP, task spawn)
- `time_format.c` — host-testable formatting helpers (no ESP32 deps)

**Public API**:

```c
esp_err_t time_sync_init(const device_config_t *config);
// Configure esp_sntp with config->ntp_server; apply POSIX TZ via setenv/tzset.

void time_sync_start(void);
// Spawn ntp_task.

time_sync_state_t time_sync_get_state(void);
// Returns TIME_STATE_SYNCED or TIME_STATE_NOT_SYNCED.

bool time_sync_get_local_time(struct tm *out_tm);
// Fills *out_tm with current local time via localtime_r(). Returns true if synced.
// Safe to call from any task — no mutex required.

int time_format_hms(const struct tm *t, char *buf, size_t len);
// Format as "HH:MM:SS". Returns chars written (excluding NUL). Host-testable.

int time_format_date(const struct tm *t, char *buf, size_t len);
// Format as "Mon DD Mon" e.g. "Sun 03 Aug". Host-testable.
```

**Usage example**:
```c
ESP_ERROR_CHECK(time_sync_init(&config));
time_sync_start();

// In clock_task:
struct tm t;
if (time_sync_get_local_time(&t)) {
    time_format_hms(&t, payload.time_str, sizeof(payload.time_str));
    time_format_date(&t, payload.date_str, sizeof(payload.date_str));
}
```

---

## `weather_service`

**Directory**: `components/weather_service/`  
**Purpose**: Periodic HTTPS fetch from OpenWeatherMap One Call 3.0, cJSON parsing into
`weather_data_t`, stale-data retention on failure, and freshness tracking.

**FreeRTOS task**: `weather_task` — priority 3, 6 KB stack (larger for TLS + cJSON stack
frames). Waits on `WIFI_CONNECTED_BIT` before first fetch.

**Source files**:
- `weather_service.c` — ESP32 platform code (HTTP client, task, staleness logic)
- `weather_parser.c` — host-testable JSON parser (no ESP32 deps, guarded by `#ifdef HOST_BUILD`)

**Public API**:

```c
esp_err_t weather_service_init(const device_config_t *config, app_state_t *state);
// Store config and state pointers; construct HTTPS request URL.

void weather_service_start(void);
// Spawn weather_task.

esp_err_t weather_parse(const char *json, size_t len, weather_data_t *out);
// Parse One Call 3.0 JSON into *out. Host-testable.
// Returns: ESP_OK on success; ESP_ERR_INVALID_RESPONSE on any rejection rule.
// Does NOT modify *out on failure — caller retains prior data.
```

**Rejection rules** (enforced by `weather_parse`):
- Missing or empty `current` object → reject
- Empty `hourly` array → reject
- `current.weather[0].id == 0` → reject
- Any required field absent → reject

**HTTP error handling** (in `weather_task`):
- HTTP 401 → `ESP_LOGE` + mark `WEATHER_UNAVAILABLE`
- HTTP 429 → `ESP_LOGW` + back-off
- HTTP 5xx → `ESP_LOGW` + retain stale data
- Timeout / TLS error → `ESP_LOGW` + retain stale data

**Usage example**:
```c
ESP_ERROR_CHECK(weather_service_init(&config, app_state_get()));
weather_service_start();
```

---

## `display`

**Directory**: `components/display/`  
**Purpose**: All OLED rendering. Wraps the `esp-idf-ssd1306` driver. Accepts only a
`display_payload_t` struct — zero direct calls to any network, NTP, or weather API
(Constitution Principle V).

**FreeRTOS task**: None — `display_render()` is called from `clock_task` every second.

**Public API**:

```c
esp_err_t display_init(void);
// Init I2C bus and SSD1306 driver; show splash screen "Kronos v0.1.0" within 2 s.

void display_render(const display_payload_t *payload);
// Render all payload fields to OLED. Uses partial redraw for time digit area.
// Safe to call from clock_task every second.

void display_show_message(const char *line1, const char *line2);
// Two-line status overlay — used during boot milestones and error states.
```

**`display_payload_t` fields**:

| Field | Type | Content |
|---|---|---|
| `time_str` | `char[]` | `"HH:MM:SS"` |
| `date_str` | `char[]` | `"Mon DD Mon"` |
| `weather_summary` | `char[]` | `"21°C  Few clouds"` or `"Weather N/A"` |
| `forecast[DISPLAY_FORECAST_LINES]` | `char[][]` | `"15:00  19°C  Rain"` per slot |
| `status` | `char[]` | Footer status string (see `build_status_string`) |
| `weather_stale` | `bool` | True when `weather.freshness == WEATHER_STALE` |

**Usage example**:
```c
ESP_ERROR_CHECK(display_init());

// In clock_task:
display_payload_t payload = {0};
snprintf(payload.time_str, sizeof(payload.time_str), "%s", time_buf);
snprintf(payload.status,   sizeof(payload.status),   "%s", status_buf);
display_render(&payload);
```

---

## `status_builder` (main helper)

**File**: `main/status_builder.c` / `main/status_builder.h`  
**Purpose**: Derive the display footer string from `wifi_state`, `time_state`, and
`weather.freshness`. Extracted from `main.c` for host testability.

**Function**:
```c
void build_status_string(wifi_state_t wifi, time_sync_state_t time,
                         weather_freshness_t freshness,
                         char *out, size_t len);
```

**Priority table** (first match wins):

| Condition | Output |
|---|---|
| `wifi == WIFI_STATE_CONNECTING` | `"Wi-Fi: connecting"` |
| `wifi == WIFI_STATE_OFFLINE` and `time == TIME_STATE_NOT_SYNCED` | `"Offline, no sync"` |
| `wifi == WIFI_STATE_OFFLINE` | `"Wi-Fi: offline"` |
| Connected + `time == TIME_STATE_NOT_SYNCED` | `"Time: syncing"` |
| Connected + synced + `WEATHER_STALE` | `"Weather stale"` |
| Connected + synced + `WEATHER_UNAVAILABLE` | `"Weather N/A"` |
| Connected + synced + `WEATHER_FRESH` | `"Synced"` |
