# Research: Kronos MVP — 001-mvp-clock

**Branch**: `001-mvp-clock` | **Date**: 2026-08-03

All NEEDS CLARIFICATION items from the Technical Context have been resolved below.

---

## 1. Weather API — Provider and Response Format

**Decision**: OpenWeatherMap **One Call API 3.0** as the reference implementation.

**Rationale**:
- Provides current conditions + hourly forecast in a single HTTPS request (FR-007, FR-009).
- Returns `weather[0].id` (condition code), `weather[0].description` (English text), and
  `temp` in each hourly slot — mapping directly to `WeatherData` entity fields.
- Free tier: 1,000 calls/day at default 15-min refresh = ~96 calls/day — well within quota.
- API key and base URL are fully configurable; any compatible REST endpoint may be swapped
  in without code changes (Constitution XII, Principle III).

**Relevant fields** (current):
```json
{
  "current": {
    "dt": 1722700000,
    "temp": 21.5,
    "weather": [{ "id": 801, "description": "few clouds" }]
  },
  "hourly": [
    { "dt": 1722700000, "temp": 21.5, "weather": [{ "id": 801, "description": "few clouds" }] },
    ...
  ]
}
```
Parser extracts `current` + up to 6 `hourly` entries. `weather[0].id` is retained as
`condition_code` per FR-008 to support future icon rendering.

**Alternatives considered**:
- *Open-Meteo*: No API key required but uses WMO condition codes (less documentation
  parity with spec). Remains a valid swap-in via `app_config` URL.
- *WeatherAPI.com*: Compatible structure but introduces another key format — deferred to
  future provider substitution.

**HTTP request pattern**:
```
GET https://api.openweathermap.org/data/3.0/onecall
  ?lat={lat}&lon={lon}&exclude=minutely,daily,alerts
  &units={units}&appid={api_key}
```
TLS via `esp_http_client` with ESP-IDF certificate bundle (`CONFIG_ESP_TLS_USING_MBEDTLS`).

---

## 2. JSON Parser — Memory-Safe Approach on ESP32

**Decision**: `cJSON` via the built-in `esp-cjson` ESP-IDF component.

**Rationale**:
- Ships with ESP-IDF — no extra component footprint.
- Simple DOM-style API; parsing a One Call response (~4–8 KB JSON body) requires one
  `cJSON_Parse()` call and per-field lookups.
- Heap use is bounded by the JSON buffer + parse tree; a 16 KB input buffer covers the
  relevant payload (current + 6 hourly slots) with margin.
- `cJSON_Delete()` on the root node frees the entire tree — no leak risk if called at
  every exit path.

**Memory estimate**:
- Input buffer: 16 KB (heap, allocated once per fetch cycle, freed after parse).
- cJSON parse tree: ~3–5× input size worst case → ~80 KB peak; acceptable within 520 KB
  SRAM when measured empirically with `ESP.getFreeHeap()` during integration testing.
  If heap proves tight, buffer can be trimmed by excluding `minutely`/`daily`/`alerts`
  in the API query (`exclude=` parameter already planned).

**Alternatives considered**:
- *jsmn*: Zero-allocation token scanner — lower peak heap but requires manual index
  arithmetic making the parser harder to read and audit. Kept as a fallback if memory
  budget forces a change.
- *Streaming SAX parser*: Lowest memory but significant implementation complexity for
  MVP. Noted as a future optimisation if cJSON heap use exceeds budget.

---

## 3. OLED Display Driver

**Decision**: `esp-idf-ssd1306` by nopnop2002, sourced via IDF Component Manager.

**Rationale**:
- Supports SSD1306 and SH1106 (both I2C and SPI) — covers the two most common OLED
  modules used with ESP32.
- Actively maintained ESP-IDF component; well-documented example code.
- Exposes a character/font API (`ssd1306_display_text`) as well as low-level pixel ops,
  supporting partial-redraw for the time digit area (Constitution V, XI).
- Driver is entirely contained within the `display` component; business logic has zero
  dependency on it.

**Integration**:
```yaml
# idf_component.yml
dependencies:
  nopnop2002/esp-idf-ssd1306: ">=1.0.0"
```
The `display` component wraps the driver and exposes only `display_init()` and
`display_render(const DisplayPayload *payload)` to the rest of the firmware.

**Alternatives considered**:
- *LVGL*: Feature-rich UI framework but adds ~100–200 KB RAM/flash overhead — unacceptable
  for MVP on a device with 520 KB SRAM. Retained as a post-MVP upgrade path once display
  memory constraints are characterised.
- *Custom framebuffer driver*: Maximum control but not warranted at MVP given the simple
  text/icon layout.

---

## 4. FreeRTOS Task Architecture

**Decision**: Four dedicated FreeRTOS tasks with shared `app_state` protected by a mutex.

| Task | Priority | Stack | Responsibility |
|---|---|---|---|
| `clock_task` | 5 (high) | 4 KB | Updates time string every 1 s; calls `display_render()` |
| `wifi_task` | 4 | 4 KB | Wi-Fi event handler; drives reconnect with exponential back-off |
| `ntp_task` | 3 | 3 KB | Triggered by Wi-Fi connect event; periodic re-sync every 3 h |
| `weather_task` | 3 | 6 KB | Periodic HTTP fetch + parse on configurable interval |

**Rationale**:
- `clock_task` at highest priority ensures the display never stalls waiting for network ops.
- `weather_task` gets a larger stack because `esp_http_client` + TLS + cJSON all consume
  stack frames during execution.
- `app_state` mutex (`xSemaphoreCreateMutex`) protects all writes; `clock_task` reads
  with a short timeout (5 ms) to avoid priority inversion stalling the display.
- FreeRTOS `EventGroupBits` signal Wi-Fi connected/disconnected across tasks.

**Alternatives considered**:
- *Single-task + callbacks*: Simpler code but weather/NTP blocking would freeze the clock.
- *Three tasks (merge ntp + weather)*: Slightly simpler but NTP sync on reconnect could
  delay the first weather fetch; kept separate for cleaner state management.

---

## 5. Host Test Harness

**Decision**: ESP-IDF native `unity` component with a `linux` CMake target for host tests.

**Rationale**:
- ESP-IDF v5 supports `idf.py -T` host tests for components marked `linux` target;
  Unity is bundled and no extra toolchain install is needed.
- Platform-independent source files (`weather_parser.c`, `time_format.c`,
  `app_config_validate.c`) are compiled with no ESP32 headers via a guard pattern:
  ```c
  #ifndef ESP_PLATFORM
  #include "host_stubs.h"  /* minimal stubs for esp_err_t, ESP_LOGI, etc. */
  #endif
  ```
- `tests/host/CMakeLists.txt` links only the platform-agnostic `.c` files plus Unity;
  runs on any CI host without hardware.

**Alternatives considered**:
- *Google Test (C++)* — heavier dependency; Unity already in ESP-IDF toolchain.
- *Separate CMake project* (fully decoupled from IDF) — possible but requires duplicating
  the ESP-IDF stub headers. The `linux` target approach reuses the IDF mock layer.

---

## 6. Configuration and Secrets Management

**Decision**: Two-layer approach — `secrets.h` (gitignored) for build-time provisioning;
NVS for runtime-mutable config.

**Build-time** (initial flash):
```c
// secrets.h (gitignored — see secrets.h.example)
#define CONFIG_WIFI_SSID     "MyNetwork"
#define CONFIG_WIFI_PASSWORD "my-password"
#define CONFIG_OWM_API_KEY   "abc123..."
```

**Runtime** (NVS namespace `kronos_cfg`):
All values from `secrets.h` are written to NVS on first boot; subsequent boots read from
NVS, allowing in-field config updates without reflashing.

**Rationale**:
- `secrets.h.example` committed as documentation (Constitution VII).
- NVS survives firmware OTA updates (dual-partition layout).
- `app_config` is the sole module that touches NVS and `secrets.h`; all others call
  `app_config_get_*()` accessors.

**Non-configurable at runtime** (compile-time only):
- `CONFIG_LOG_DEFAULT_LEVEL` — set via `sdkconfig.defaults`.
- Display driver selection — determined at build time.

---

## 7. Memory Budget Estimate

| Region | Item | Estimate |
|---|---|---|
| Heap (peak) | cJSON parse buffer + tree | ~100 KB (during fetch only) |
| Heap (steady) | `app_state` + `DeviceConfig` structs | ~2 KB |
| Heap (steady) | TLS session (mbedTLS) | ~36 KB |
| Stack | 4 FreeRTOS tasks × avg 4 KB | ~17 KB total |
| Flash (code) | Application + components | ~600–800 KB estimated |
| Flash (OTA) | Two OTA partitions + NVS | see partition-table.csv |

Peak heap during weather fetch: ~138 KB. Available SRAM after FreeRTOS kernel overhead
(~50 KB) and IDF Wi-Fi stack (~100 KB): ~370 KB available. **Sufficient with margin.**
`idf.py size` review required after first build; heap watermark to be checked via
`esp_get_minimum_free_heap_size()` in integration tests.

---

## Resolved Unknowns Summary

| Was NEEDS CLARIFICATION | Resolution |
|---|---|
| Weather API provider and response shape | OpenWeatherMap One Call 3.0 |
| JSON parser choice and memory safety | cJSON (esp-cjson, built-in) |
| OLED driver component | esp-idf-ssd1306 (nopnop2002) |
| FreeRTOS task structure | 4 tasks: clock, wifi, ntp, weather |
| Host test framework | ESP-IDF Unity linux target |
| Config/secrets management | secrets.h (gitignored) + NVS |
| Memory budget viability | ~138 KB peak heap — within 520 KB SRAM budget |
