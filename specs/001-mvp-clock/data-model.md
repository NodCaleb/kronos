# Data Model: Kronos MVP — 001-mvp-clock

**Branch**: `001-mvp-clock` | **Date**: 2026-08-03

---

## Overview

Four core data structures flow through the firmware. All are plain C structs; no heap
allocation in hot paths. All inter-module sharing is done through `app_state` accessors
protected by a FreeRTOS mutex.

```
DeviceConfig ──read-by──► app_state
                               │
                        writes▼
                         AppState ──read-by──► clock_task
                               │                    │
                    read-by    │               builds▼
                  weather_task │           DisplayPayload
                  ntp_task     │                    │
                               │               renders▼
                         writes▼              display module
                         WeatherData
```

---

## Entity: `WeatherData`

Represents the most recently fetched weather snapshot, stored in `app_state`.

```c
// components/weather_service/include/weather_service.h

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
```

**Validation rules**:
- `forecast_count` MUST be 0–6; parser rejects payloads with no hourly array.
- `condition_code` MUST be > 0; zero treated as parse failure.
- `fetch_timestamp_s` set from `time(NULL)` at successful parse completion.
- On parse failure: `weather_data_t` is NOT updated; existing data retained (FR-014).

**State transitions**:
```
UNAVAILABLE ──first successful fetch──► FRESH
FRESH       ──age > 2×interval────────► STALE
STALE       ──successful fetch──────────► FRESH
FRESH/STALE ──failed fetch─────────────► (unchanged freshness, fetch_timestamp unchanged)
```

---

## Entity: `AppState`

Shared runtime state for the device. Single instance, mutex-protected in `app_state.c`.

```c
// components/app_state/include/app_state.h

typedef enum {
    WIFI_STATE_INIT,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_OFFLINE
} wifi_state_t;

typedef enum {
    TIME_STATE_NOT_SYNCED,
    TIME_STATE_SYNCED
} time_sync_state_t;

typedef struct {
    wifi_state_t       wifi_state;
    time_sync_state_t  time_state;
    struct tm          local_time;         /* updated every second by clock_task */
    weather_data_t     weather;            /* last valid weather; freshness field governs display */
    int64_t            last_weather_update_s;
    char               status_message[64]; /* human-readable status for display footer */
} app_state_t;
```

**Access pattern**:
- Writers (`wifi_task`, `ntp_task`, `weather_task`): acquire mutex, update field, release.
- Reader (`clock_task`): acquire mutex with 5 ms timeout, copy relevant fields, release.
  Proceeds with last known copy if mutex not acquired within timeout.

---

## Entity: `DeviceConfig`

All user-configurable settings. Loaded once at boot from NVS by `app_config`; immutable
during runtime (no hot-reload in MVP). Passed to modules at init time via pointer.

```c
// components/app_config/include/app_config.h

#define CONFIG_WIFI_SSID_LEN        33
#define CONFIG_WIFI_PASSWORD_LEN    65
#define CONFIG_NTP_SERVER_LEN       64
#define CONFIG_TZ_STRING_LEN        48
#define CONFIG_WEATHER_URL_LEN     128
#define CONFIG_WEATHER_API_KEY_LEN  64
#define CONFIG_WEATHER_LOCATION_LEN 32

typedef enum {
    UNITS_METRIC,
    UNITS_IMPERIAL
} measurement_units_t;

typedef struct {
    /* Network */
    char                wifi_ssid[CONFIG_WIFI_SSID_LEN];
    char                wifi_password[CONFIG_WIFI_PASSWORD_LEN];
    uint32_t            wifi_max_retry_interval_s;  /* exponential back-off cap, default 300 s */

    /* Time */
    char                ntp_server[CONFIG_NTP_SERVER_LEN];    /* default: pool.ntp.org */
    char                tz_posix[CONFIG_TZ_STRING_LEN];        /* POSIX TZ string e.g. "EST5EDT,M3.2.0,M11.1.0" */

    /* Weather */
    char                weather_api_url[CONFIG_WEATHER_URL_LEN];
    char                weather_api_key[CONFIG_WEATHER_API_KEY_LEN];
    char                weather_location[CONFIG_WEATHER_LOCATION_LEN]; /* lat,lon */
    measurement_units_t units;
    uint32_t            weather_refresh_interval_s;  /* default: 900 s (15 min) */
    uint32_t            request_timeout_ms;          /* default: 10000 ms */

    /* Diagnostics */
    uint8_t             log_level;  /* maps to esp_log_level_t */
} device_config_t;
```

**Validation rules** (enforced by `app_config_validate()`; host-testable):
- `wifi_ssid` MUST be non-empty.
- `wifi_max_retry_interval_s` MUST be > 0, ≤ 3600.
- `ntp_server` MUST be non-empty.
- `tz_posix` MUST be non-empty.
- `weather_api_url` MUST start with `https://`.
- `weather_api_key` MUST be non-empty.
- `weather_refresh_interval_s` MUST be in range [60, 3600].
- `request_timeout_ms` MUST be in range [1000, 30000].

**Derived values** (not stored, computed on access):
- `weather_stale_threshold_s = 2 × weather_refresh_interval_s` (FR-017).

---

## Entity: `DisplayPayload`

Structured data passed from `clock_task` to `display_render()`. Contains only
pre-formatted strings; no raw network data or config values (FR-019).

```c
// components/display/include/display.h

#define DISPLAY_TIME_LEN        9    /* "HH:MM:SS\0" */
#define DISPLAY_DATE_LEN        12   /* "Mon DD Mon\0" */
#define DISPLAY_WEATHER_LEN     24
#define DISPLAY_FORECAST_LINES  3
#define DISPLAY_FORECAST_LEN    24
#define DISPLAY_STATUS_LEN      20

typedef struct {
    char time_str[DISPLAY_TIME_LEN];           /* "14:32:07" */
    char date_str[DISPLAY_DATE_LEN];           /* "Sun 03 Aug" */
    char weather_summary[DISPLAY_WEATHER_LEN]; /* "21°C  Few clouds" or "Weather N/A" */
    char forecast[DISPLAY_FORECAST_LINES][DISPLAY_FORECAST_LEN]; /* "15:00  19°C  Rain" */
    char status[DISPLAY_STATUS_LEN];           /* "Wi-Fi: offline" */
    bool weather_stale;                        /* drives staleness indicator on display */
} display_payload_t;
```

**Population rules**:
- `time_str` populated from `app_state.local_time` every second.
- `date_str` populated from `app_state.local_time` every second.
- `weather_summary` populated from `app_state.weather`; shows "Weather N/A" when
  `freshness == WEATHER_UNAVAILABLE`.
- `forecast` lines populated for up to `DISPLAY_FORECAST_LINES` hourly slots.
- `status` derived from `app_state.wifi_state` + `app_state.time_state` +
  `app_state.weather.freshness`; maps to one of the seven display states (SC-005).
- `weather_stale` set when `freshness == WEATHER_STALE`.

---

## Status State Machine (SC-005, FR-006, FR-015, FR-016)

The `status` string and display indicator map to exactly seven states:

| `app_state` condition | `status` string | `weather_stale` |
|---|---|---|
| `wifi_state == CONNECTING` | `"Wi-Fi: connecting"` | — |
| `wifi_state == CONNECTED, time_state == NOT_SYNCED` | `"Time: syncing"` | — |
| `wifi_state == CONNECTED, time_state == SYNCED, weather == FRESH` | `"Synced"` | false |
| `wifi_state == CONNECTED, time_state == SYNCED, weather == STALE` | `"Weather stale"` | true |
| `wifi_state == CONNECTED, time_state == SYNCED, weather == UNAVAILABLE` | `"Weather N/A"` | — |
| `wifi_state == OFFLINE, time_state == SYNCED` | `"Wi-Fi: offline"` | (preserved) |
| `wifi_state == OFFLINE, time_state == NOT_SYNCED` | `"Offline, no sync"` | — |

Priority: Wi-Fi state shown first; time sync state second; weather state last (never shown if
Wi-Fi is offline/connecting since that is the more critical status).
