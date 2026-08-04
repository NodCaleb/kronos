---
description: "Implementation tasks for Kronos MVP — Embedded IoT Digital Clock with Weather Display"
---

# Tasks: Kronos MVP — Embedded IoT Digital Clock with Weather Display

**Input**: Design documents from `/specs/001-mvp-clock/`

**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓, quickstart.md ✓

**Tests**: Host-compiled Unity tests included for `weather_parser`, `time_format`, and `app_config_validate` — required by User Story 7. On-device integration test stubs included per quickstart.md Scenario 7.

**Organization**: Tasks grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on other in-progress tasks)
- **[Story]**: Which user story this task belongs to (US1–US8)
- Paths follow ESP-IDF component layout per plan.md — all paths relative to repo root

---

## Phase 1: Setup (Project Infrastructure)

**Purpose**: Repository scaffolding and ESP-IDF project initialization. No component code begins until this phase is complete.

- [ ] T001 Initialize ESP-IDF project directory structure per plan.md: create `main/`, `components/`, `tests/host/`, `tests/integration/`, `docs/` at repository root
- [ ] T002 [P] Create `partition-table.csv` at repo root with app0, app1 (OTA), nvs, and phy_init partitions (Principle XII — dual OTA from day one)
- [ ] T003 [P] Create `idf_component.yml` at repo root pinning ESP-IDF v5.x and declaring `nopnop2002/esp-idf-ssd1306: ">=1.0.0"` as a dependency
- [ ] T004 [P] Create `sdkconfig.defaults` at repo root with log level default, I2C pin defaults (SDA=21, SCL=22), `CONFIG_ESP_TLS_USING_MBEDTLS=y`, and `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y`
- [ ] T005 [P] Create `main/secrets.h.example` with placeholder `#define` entries for `CONFIG_WIFI_SSID`, `CONFIG_WIFI_PASSWORD`, `CONFIG_OWM_API_KEY`, `CONFIG_WEATHER_LOCATION`, and `CONFIG_TZ_POSIX`
- [ ] T006 [P] Create `.gitignore` at repo root excluding `main/secrets.h`, `sdkconfig.local`, `build/`, and any generated IDE files
- [ ] T007 [P] Create `main/CMakeLists.txt` registering `main.c` and declaring all eight component dependencies

**Checkpoint**: `idf.py set-target esp32 && idf.py build` parses the project structure without component source errors.

---

## Phase 2: Foundational (Shared Core — Blocks All User Stories)

**Purpose**: The three shared infrastructure components (`app_config`, `app_state`, `error_handler`) must exist before any module can compile or be tested.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T008 Create `components/app_config/include/app_config.h` declaring `measurement_units_t` enum, `device_config_t` struct with all fields and `CONFIG_*_LEN` constants from data-model.md, and `app_config_load()` / `app_config_validate()` prototypes
- [ ] T009 Implement `app_config_validate()` (host-testable, no ESP32 platform deps) in `components/app_config/app_config.c` enforcing all field validation rules from data-model.md: non-empty SSID, URL must start with `https://`, `weather_refresh_interval_s` in [60, 3600] s, `request_timeout_ms` in [1000, 30000] ms, etc.
- [ ] T010 Implement `app_config_load()` in `components/app_config/app_config.c` — read from NVS namespace `kronos_cfg`; on first boot write values from `secrets.h`; return `ESP_ERR_NVS_*` on storage error
- [ ] T011 [P] Create `components/app_config/CMakeLists.txt` declaring `REQUIRES nvs_flash`
- [ ] T012 Create `components/app_state/include/app_state.h` — `#include <time_sync.h>` for `time_sync_state_t` (owned by `time_sync.h`; do **not** redeclare it here); declare `wifi_state_t` enum (`WIFI_STATE_INIT/CONNECTING/CONNECTED/OFFLINE`), `app_state_t` struct (with `wifi_state`, `time_state`, `local_time`, `weather`, `last_weather_update_s`, `status_message[64]`), and all accessor prototypes from contracts/module-apis.md
- [ ] T013 Implement `app_state_init()`, `app_state_lock()`, `app_state_unlock()`, `app_state_get()`, and `app_state_try_read()` in `components/app_state/app_state.c` using a FreeRTOS mutex (`xSemaphoreCreateMutex`); `app_state_try_read()` uses 5 ms timeout
- [ ] T014 [P] Create `components/app_state/CMakeLists.txt` declaring `REQUIRES freertos`
- [ ] T015 [P] Create `components/error_handler/include/error_handler.h` declaring `error_handler_fatal(const char *tag, const char *message, esp_err_t err)` and `error_handler_warn(const char *tag, const char *message, esp_err_t err)`
- [ ] T016 [P] Implement `error_handler_fatal()` (log `ESP_LOGE`, delay 1 s, call `esp_restart()`) and `error_handler_warn()` (log `ESP_LOGW`, no restart) in `components/error_handler/error_handler.c`
- [ ] T017 [P] Create `components/error_handler/CMakeLists.txt`

**Checkpoint**: All three foundational components compile cleanly. `app_config`, `app_state`, and `error_handler` headers are importable. User story implementation can now begin.

---

## Phase 3: User Story 1 — Reliable Local Time Display (Priority: P1) 🎯 MVP

**Goal**: Device powers on, initializes the OLED display (splash screen within 2 s), and shows `HH:MM:SS` + date updating every second via `clock_task`. Time display never freezes regardless of network or weather state.

**Independent Test**: Flash firmware with valid Wi-Fi + NTP config. Observe display updates every second. While weather fetch runs, confirm time does not freeze. See quickstart.md Scenario 3.

### Implementation for User Story 1

- [ ] T018 [US1] Create `components/display/include/display.h` declaring `display_payload_t` struct with all fields and `DISPLAY_*_LEN` constants from data-model.md, plus `display_init()`, `display_render()`, and `display_show_message()` prototypes
- [ ] T019 [US1] Implement `display_init()` in `components/display/display.c` — initialize I2C bus, call `esp-idf-ssd1306` driver init, show splash screen `"Kronos v0.1.0"` via `display_show_message()` within 2 seconds of power-on (SC-001, Principle XI)
- [ ] T020 [US1] Implement `display_render(const display_payload_t *payload)` in `components/display/display.c` — render all `display_payload_t` fields to OLED; use partial redraw for time digit area; zero direct calls to `esp_sntp`, `esp_http_client`, or any `weather_*` function (Principle V)
- [ ] T021 [P] [US1] Implement `display_show_message(const char *line1, const char *line2)` in `components/display/display.c` — two-line status overlay for boot and error states
- [ ] T022 [P] [US1] Create `components/display/CMakeLists.txt` declaring `REQUIRES nopnop2002__esp-idf-ssd1306`
- [ ] T023 [US1] Create `components/time_sync/include/time_sync.h` declaring `time_sync_state_t` enum and all prototypes from contracts/module-apis.md: `time_sync_init()`, `time_sync_start()`, `time_sync_get_state()`, `time_sync_get_local_time()`, `time_format_hms()`, `time_format_date()`
- [ ] T024 [P] [US1] Implement `time_format_hms(const struct tm *t, char *buf, size_t len)` (host-testable — no ESP32 platform deps) in `components/time_sync/time_format.c` formatting `struct tm` as `"HH:MM:SS"`; add `#ifndef ESP_PLATFORM` guard
- [ ] T025 [P] [US1] Implement `time_format_date(const struct tm *t, char *buf, size_t len)` (host-testable) in `components/time_sync/time_format.c` formatting `struct tm` as `"%a %d %b"` (e.g., `"Sun 03 Aug"`); guard with `#ifndef ESP_PLATFORM`
- [ ] T026 [P] [US1] Create `components/time_sync/CMakeLists.txt` declaring `REQUIRES esp_sntp`
- [ ] T027 [US1] Create `main/main.c` with `app_main()` calling `app_config_load()`, `app_state_init()`, `display_init()`; implement `clock_task` (priority 5, 4 KB stack) that reads `app_state` every second via `app_state_try_read()`, builds `display_payload_t` (time + date via `time_format_hms` / `time_format_date`), and calls `display_render()`

**Checkpoint**: Flash firmware. Display shows `HH:MM:SS` + date updating every second. Splash screen appears at boot. US1 independently testable — clock runs even with no network.

---

## Phase 4: User Story 3 — Wi-Fi Connectivity with Automatic Recovery (Priority: P3)

**Goal**: Device connects to configured Wi-Fi on boot, handles unavailable network with exponential back-off (capped at `wifi_max_retry_interval_s`), updates `app_state.wifi_state`, and auto-recovers without user intervention when network is restored.

**Independent Test**: Boot with router off — display shows offline/retrying status, no crash, no reboot loop. Enable router — device reconnects automatically, NTP re-syncs, weather task resumes. See quickstart.md Scenario 4.

### Implementation for User Story 3

- [ ] T032 [US3] Create `components/wifi_manager/include/wifi_manager.h` declaring `wifi_state_t` enum, `WIFI_CONNECTED_BIT` / `WIFI_DISCONNECTED_BIT` event bit constants, and all four accessor prototypes from contracts/module-apis.md
- [ ] T033 [US3] Implement `wifi_manager_init(const device_config_t *config)` in `components/wifi_manager/wifi_manager.c` — initialize `esp_wifi` stack in station mode; register `WIFI_EVENT` and `IP_EVENT` handlers; create `EventGroupHandle_t`; store config pointer; return `ESP_OK` or `ESP_ERR_*`
- [ ] T034 [US3] Implement `wifi_manager_start()` spawning `wifi_task` (priority 4, 4 KB stack) in `components/wifi_manager/wifi_manager.c` — task drives connection; on connect sets `WIFI_CONNECTED_BIT`, clears `WIFI_DISCONNECTED_BIT`, updates `app_state.wifi_state = WIFI_STATE_CONNECTED`; on disconnect sets `WIFI_DISCONNECTED_BIT`, clears `WIFI_CONNECTED_BIT`, updates `app_state.wifi_state = WIFI_STATE_OFFLINE`; retries with exponential back-off (start 1 s, double each attempt, cap at `config->wifi_max_retry_interval_s`)
- [ ] T035 [P] [US3] Implement `wifi_manager_get_state()` and `wifi_manager_get_event_group()` as thread-safe accessors in `components/wifi_manager/wifi_manager.c`
- [ ] T036 [P] [US3] Create `components/wifi_manager/CMakeLists.txt` declaring `REQUIRES esp_wifi esp_event`
- [ ] T037 [US3] Update `main/main.c` `app_main()` to call `wifi_manager_init()` and `wifi_manager_start()` before `time_sync_start()`; pass `wifi_manager_get_event_group()` to `time_sync_init()` so `ntp_task` waits on the correct event group

**Checkpoint**: Boot with router off — offline/retrying status shown, no crash, no reboot loop. Re-enable router — device reconnects, NTP re-syncs, no intervention needed. US3 independently testable.

---

## Phase 5: User Story 2 — Automatic Internet Time Synchronization (Priority: P2)

**Goal**: Device connects to Wi-Fi (prerequisite), starts SNTP with configured NTP server and POSIX TZ string, syncs time, updates `app_state.time_state` to `TIME_STATE_SYNCED`, and re-syncs every 3 hours.

**Independent Test**: Boot device with valid config. Serial log shows `"Time synchronized"`. Displayed time matches reference clock. Status indicator transitions from `"Time: syncing"` to `"Synced"`. See quickstart.md Scenario 2.

### Implementation for User Story 2

- [ ] T028 [US2] Implement `time_sync_init(const device_config_t *config)` in `components/time_sync/time_sync.c` — configure `esp_sntp` with `config->ntp_server`; set POSIX TZ via `setenv("TZ", config->tz_posix, 1)` + `tzset()`; store event group handle from `wifi_manager_get_event_group()`
- [ ] T029 [US2] Implement `time_sync_start()` spawning `ntp_task` (priority 3, 3 KB stack) in `components/time_sync/time_sync.c` — task waits on `WIFI_CONNECTED_BIT`; calls `esp_sntp_init()`; polls sync status with timeout; on sync success updates `app_state.time_state = TIME_STATE_SYNCED` and logs `"Time synchronized. Drift: %.2f s"`; re-syncs every 3 hours; on NTP failure logs `ESP_LOGW` and retries without crashing
- [ ] T030 [P] [US2] Implement `time_sync_get_state()` and `time_sync_get_local_time()` accessors in `components/time_sync/time_sync.c` — `get_local_time()` reads system clock via `localtime_r()`; safe to call from any task
- [ ] T031 [US2] Update `main/main.c` `app_main()` to call `time_sync_init()` and `time_sync_start()` after `wifi_manager_start()`; update `clock_task` to include `app_state.time_state` in the `DisplayPayload.status` string

**Checkpoint**: Serial log shows NTP sync event; displayed time matches reference; status cycles `"Time: syncing"` → `"Synced"`. US2 independently testable (requires Phase 4 Wi-Fi to be functional).

---

## Phase 6: User Story 4 — Current Weather and Short Forecast Display (Priority: P4)

**Goal**: Device periodically fetches weather from OpenWeatherMap One Call 3.0 over HTTPS, parses current temp + condition + up to 6 hourly forecast slots, displays results on OLED, retains last valid data on failure, and marks weather as stale after `2 × weather_refresh_interval_s`.

**Independent Test**: Configure valid API key — weather area populates after first fetch. Set invalid API key — `"Weather N/A"` shown, time unaffected. Block internet after valid fetch — stale indicator appears after 2× interval. See quickstart.md Scenario 5.

### Implementation for User Story 4

- [ ] T038 [US4] Create `components/weather_service/include/weather_service.h` declaring `weather_freshness_t` enum (`WEATHER_FRESH/STALE/UNAVAILABLE`), `hourly_slot_t` struct, `weather_data_t` struct (with all fields and `WEATHER_*_LEN` / `WEATHER_FORECAST_SLOTS` constants from data-model.md), and prototypes for `weather_service_init()`, `weather_service_start()`, `weather_parse()`
- [ ] T039 [US4] Implement `weather_parse(const char *json, size_t len, weather_data_t *out)` (host-testable — no ESP32 platform deps) in `components/weather_service/weather_parser.c` — parse OWM One Call 3.0 JSON via cJSON; extract `current.temp`, `current.weather[0].id/description`, and up to 6 `hourly[]` slots (dt → `"HH:MM"` label, temp, condition_code/text); apply all rejection rules from contracts/weather-api.md; return `ESP_ERR_INVALID_RESPONSE` without modifying `*out` on any rejection; call `cJSON_Delete()` at every exit path; add `#ifndef ESP_PLATFORM` guard
- [ ] T040 [US4] Implement `weather_service_init(const device_config_t *config, app_state_t *state)` in `components/weather_service/weather_service.c` — store config and state pointers; construct HTTPS request URL from `config->weather_api_url`, lat/lon, exclude params, units, and API key; set `User-Agent: Kronos/1.0 ESP32`
- [ ] T041 [US4] Implement `weather_service_start()` spawning `weather_task` (priority 3, 6 KB stack) in `components/weather_service/weather_service.c` — task waits on `WIFI_CONNECTED_BIT`; fetches via `esp_http_client` with HTTPS + `CONFIG_ESP_TLS_USING_MBEDTLS` CA bundle + `request_timeout_ms`; on HTTP 200 calls `weather_parse()`, updates `app_state.weather` under mutex, sets `last_weather_update_s`; handles HTTP 401 (`ESP_LOGE` + mark UNAVAILABLE), 429 (`ESP_LOGW` + back-off), 5xx (`ESP_LOGW` + retain stale), timeout, and TLS errors per contracts/weather-api.md; repeats on `weather_refresh_interval_s` schedule
- [ ] T042 [US4] Implement staleness evaluation in `weather_task` in `components/weather_service/weather_service.c` — compute `stale_threshold_s = 2 × config->weather_refresh_interval_s`; each loop iteration: if `(time(NULL) - app_state.weather.fetch_timestamp_s) >= stale_threshold_s` set `app_state.weather.freshness = WEATHER_STALE` under mutex; set `WEATHER_FRESH` on successful parse
- [ ] T043 [P] [US4] Create `components/weather_service/CMakeLists.txt` declaring `REQUIRES esp_http_client esp-cjson app_state app_config`
- [ ] T044 [US4] Update `clock_task` in `main/main.c` to read `app_state.weather` and populate `DisplayPayload.weather_summary` (`"21°C  Few clouds"` or `"Weather N/A"` when `WEATHER_UNAVAILABLE`), `DisplayPayload.forecast[]` (up to `DISPLAY_FORECAST_LINES` slots formatted as `"15:00  19°C  Rain"`), and `DisplayPayload.weather_stale` fields; update `main/main.c` `app_main()` to call `weather_service_init()` and `weather_service_start()`

**Checkpoint**: Weather area shows current temp + condition + up to 3 forecast slots. Stale indicator appears after 2× refresh interval. Invalid API key shows `"Weather N/A"`. Time and date unaffected throughout. US4 independently testable.

---

## Phase 7: User Story 5 — Clear Device Status Feedback (Priority: P5)

**Goal**: All seven status states from the data-model.md Status State Machine (SC-005, FR-006) are surfaced as distinct, human-readable strings in `DisplayPayload.status` and rendered in the display footer. Startup progress messages appear within 2 seconds of power-on.

**Independent Test**: Cycle through all 7 states per quickstart.md Scenario 6 and verify each state produces the correct footer string from the status state machine table in data-model.md.

### Implementation for User Story 5

- [ ] T045 [US5] Implement `build_status_string()` (file-local helper) in `main/main.c` — derive `status` string from `app_state.wifi_state` + `app_state.time_state` + `app_state.weather.freshness` following the priority table in data-model.md: `"Wi-Fi: connecting"` → `"Time: syncing"` → `"Synced"` → `"Weather stale"` → `"Weather N/A"` → `"Wi-Fi: offline"` → `"Offline, no sync"` (Wi-Fi state evaluated first, weather state only when Wi-Fi is connected and time is synced)
- [ ] T046 [US5] Update `clock_task` in `main/main.c` to call `build_status_string()` every second iteration and assign the result to `payload.status`; set `payload.weather_stale = (app_state.weather.freshness == WEATHER_STALE)` before every `display_render()` call
- [ ] T047 [US5] Update `app_main()` in `main/main.c` to call `display_show_message()` at each init milestone: `"Loading config..."` before `app_config_load()`, `"Connecting Wi-Fi..."` before `wifi_manager_start()`, `"Syncing time..."` before `time_sync_start()`; confirm splash + first message appears within 2 seconds of power-on (Principle XI)

**Checkpoint**: All 7 status states produce distinct, correct footer strings per the quickstart.md Scenario 6 table. Splash visible within 2 seconds. US5 independently testable by state cycling.

---

## Phase 8: User Story 6 — Modular, Maintainable Firmware Architecture (Priority: P6)

**Goal**: All eight modules have clean public headers with minimal surface area, no module directly accesses another's internals, `display.c` contains zero network/NTP/weather API calls, and `main.c` is the sole module-wiring point (Principle VI).

**Independent Test**: Code review — each component has exactly one `.h` header; `display.c` has no `esp_sntp`, `esp_http_client`, or `weather_*` calls; no `.c`-to-`.c` includes; `main.c` top-of-`app_main` comment documents full init order.

### Implementation for User Story 6

- [ ] T048 [P] [US6] Audit `components/display/display.c` — verify zero direct calls to `esp_sntp`, `esp_http_client`, NTP or `weather_*` functions; fix any violations so display module accepts only `DisplayPayload` input (Principle V)
- [ ] T049 [P] [US6] Audit all component `CMakeLists.txt` files — verify each component lists only its direct dependencies in `REQUIRES`; confirm no circular dependencies exist in the component graph (Principle VI)
- [ ] T050 [P] [US6] Audit all component `.h` headers — verify each exposes only types defined in its own header or explicitly `#include`d headers; no internal structs or file-scope variables in public headers
- [ ] T051 [US6] Add a wiring-order comment block at the top of `app_main()` in `main/main.c` documenting the init sequence and which module each `_init()` / `_start()` call belongs to; confirm `main.c` is the sole location where all eight module `_init()` and `_start()` functions are called

**Checkpoint**: Code review passes all six criteria. No constitution violations for Principle V or VI. US6 independently verifiable by code review.

---

## Phase 9: User Story 7 — Host-Testable Business Logic (Priority: P7)

**Goal**: `weather_parse()`, `time_format_hms()`, `time_format_date()`, and `app_config_validate()` are covered by host-compiled Unity tests that run on any development machine without ESP32 hardware. All tests pass before any firmware is flashed.

**Independent Test**: `cd tests/host && cmake -B build && cmake --build build && ./build/host_tests` — all Unity tests report PASS. See quickstart.md Scenario 1.

### Implementation for User Story 7

- [ ] T052 [US7] Create `tests/host/CMakeLists.txt` as a standalone CMake project (no `idf.py` required): links `../../components/weather_service/weather_parser.c`, `../../components/time_sync/time_format.c`, `../../components/app_config/app_config.c` (validation path only) + Unity source; produces executable `host_tests`; sets `-DHOST_BUILD=1` compile definition
- [ ] T053 [P] [US7] Create `tests/host/host_stubs.h` providing minimal stubs for host compilation: `typedef int esp_err_t`, `#define ESP_OK 0`, `#define ESP_ERR_INVALID_ARG -1`, `#define ESP_ERR_INVALID_RESPONSE -2`, no-op `ESP_LOGI/LOGW/LOGE` macros; guards with `#ifndef HOST_STUBS_H`
- [ ] T054 [P] [US7] Update `#ifndef ESP_PLATFORM` guards in `components/weather_service/weather_parser.c`, `components/time_sync/time_format.c`, and `app_config_validate()` in `components/app_config/app_config.c` to `#ifdef HOST_BUILD` (or ensure both guards compile cleanly on host without ESP-IDF headers)
- [ ] T055 [US7] Create `tests/host/test_weather_parser.c` with Unity tests: (1) valid OWM JSON parses correctly — `current_temp`, `current_condition_code`, 6 `hourly_slot_t` entries; (2) empty `hourly` array returns `ESP_ERR_INVALID_RESPONSE`; (3) missing `current` object returns `ESP_ERR_INVALID_RESPONSE`; (4) `*out` is unchanged on any parse failure (retain-old-data rule from data-model.md); (5) `current.weather` with `id == 0` rejected
- [ ] T056 [P] [US7] Create `tests/host/test_time_format.c` with Unity tests: `time_format_hms()` formats midnight as `"00:00:00"`, noon as `"12:00:00"`, 23:59:59 as `"23:59:59"`; `time_format_date()` formats a known `struct tm` to the correct `"%a %d %b"` output (e.g., `"Sun 03 Aug"`); buffer too-small returns expected char count
- [ ] T057 [P] [US7] Create `tests/host/test_app_config.c` with Unity tests: empty `wifi_ssid` returns `ESP_ERR_INVALID_ARG`; `weather_api_url` starting with `http://` returns `ESP_ERR_INVALID_ARG`; `weather_refresh_interval_s = 59` returns `ESP_ERR_INVALID_ARG`; `weather_refresh_interval_s = 3601` returns `ESP_ERR_INVALID_ARG` (SC-007 upper bound); `request_timeout_ms = 31000` returns `ESP_ERR_INVALID_ARG`; fully valid config returns `ESP_OK`
- [ ] T067 [US7] Extract `build_status_string()` from `main/main.c` into `main/status_builder.c` + `main/status_builder.h`; add `#ifdef HOST_BUILD` guard (no FreeRTOS or ESP-IDF hardware deps); update `main/main.c` to `#include "status_builder.h"`; update `tests/host/CMakeLists.txt` to also link `../../main/status_builder.c` (resolves C1 — makes display data preparation host-testable per SC-009/US7)
- [ ] T068 [P] [US7] Create `tests/host/test_status_builder.c` with Unity tests covering all 7 status strings per the data-model.md Status State Machine: `WIFI_STATE_CONNECTING` → `"Wi-Fi: connecting"`; connected + `TIME_STATE_NOT_SYNCED` → `"Time: syncing"`; connected + synced + `WEATHER_FRESH` → `"Synced"`; connected + synced + `WEATHER_STALE` → `"Weather stale"`; connected + synced + `WEATHER_UNAVAILABLE` → `"Weather N/A"`; `WIFI_STATE_OFFLINE` + synced → `"Wi-Fi: offline"`; offline + not synced → `"Offline, no sync"`

**Checkpoint**: `./tests/host/build/host_tests` reports all tests PASS (including T068 status builder). No hardware required. US7 independently executable on any host with cmake + gcc/clang.

---

## Phase 10: User Story 8 — Comprehensive Project Documentation (Priority: P8)

**Goal**: Four files in `docs/` covering architecture, modules, testing, and deployment — sufficient for an unfamiliar ESP-IDF/C developer to onboard, build, test, flash, and update configuration without asking clarifying questions.

**Independent Test**: Provide docs to a developer unfamiliar with the project; they can complete all steps in quickstart.md using only the documentation.

### Implementation for User Story 8

- [ ] T058 [P] [US8] Create `docs/architecture.md` documenting: technology stack (C17, ESP-IDF v5.x, FreeRTOS, cJSON, SSD1306 driver); all 8 component modules with one-line responsibility summaries; inter-module dependency graph (acyclic, per plan.md); 4 FreeRTOS tasks with priorities and stack sizes (from research.md §4); peak memory budget (from research.md §7); dual-OTA partition layout
- [ ] T059 [P] [US8] Create `docs/modules.md` documenting each of the 8 modules: purpose, public API summary (from contracts/module-apis.md), FreeRTOS task it owns (if any), key behaviors, and a brief usage example showing how to call its init/start/accessor functions
- [ ] T060 [P] [US8] Create `docs/testing.md` documenting: host test setup and run commands (`cmake -B build && cmake --build build && ./build/host_tests`); expected host test output (from quickstart.md Scenario 1); on-device integration test commands (`idf.py -T tests/integration/...`); expected serial output for each integration test
- [ ] T061 [P] [US8] Create `docs/deployment.md` documenting: `secrets.h` setup from `secrets.h.example`; full build/flash/monitor command sequence (`idf.py set-target esp32`, `menuconfig`, `build`, `flash`, `monitor`); NVS config update procedure; expected serial output on successful boot (from quickstart.md Scenario 2); OTA update path note referencing dual-partition layout

**Checkpoint**: All four `docs/` files exist, cover their scope, and match final implemented API shapes. US8 deliverable complete.

---

## Final Phase: Polish & Cross-Cutting Concerns

**Purpose**: On-device integration test scaffolding, observability audit, and full quickstart validation.

- [ ] T062 [P] Create on-device integration test project `tests/integration/test_wifi_manager/` with `CMakeLists.txt` and a Unity test verifying `wifi_manager_init()` returns `ESP_OK` given a valid `device_config_t`
- [ ] T063 [P] Create on-device integration test project `tests/integration/test_time_sync/` with `CMakeLists.txt` and a Unity test verifying `time_sync_init()` returns `ESP_OK` and `time_sync_get_state()` returns `TIME_STATE_NOT_SYNCED` before any NTP sync
- [ ] T064 [P] Create on-device integration test project `tests/integration/test_weather_service/` with `CMakeLists.txt` and a Unity test verifying `weather_service_init()` returns `ESP_OK` and initial `app_state.weather.freshness` is `WEATHER_UNAVAILABLE`
- [ ] T065 Audit `ESP_LOGI/W/E` log coverage across all 8 components — confirm all significant events are logged (Principle VIII): Wi-Fi connect/disconnect events, NTP sync start/success/failure, weather fetch start/success/failure (with HTTP status code or error description), config load, app start banner `"Booting Kronos v0.1.0"`
- [ ] T066 Run full quickstart.md validation checklist (Scenarios 1–7) and confirm all 8 checkboxes pass including SC-008 (`git grep` credentials check)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 — **BLOCKS all user stories**
- **Phase 3–10 (User Stories)**: All depend on Phase 2 completion; proceed in priority order or parallel by story
- **Phase 5 (US2 — Time Sync)**: Depends on Phase 4 (US3 — Wi-Fi); `ntp_task` calls `wifi_manager_get_event_group()` created in Phase 4
- **Polish**: Depends on all desired user stories being complete

### User Story Dependencies

| Story | Hard Dependencies | Notes |
|---|---|---|
| US1 (P1) | Phase 2 | `display` + `time_format.c` — no network needed |
| US2 (P2) | US3 (Wi-Fi event group), US1 (display shows sync state) | `ntp_task` waits on `WIFI_CONNECTED_BIT` |
| US3 (P3) | Phase 2 | `wifi_manager` is independent; provides event group to US2 + US4 |
| US4 (P4) | US3 (WIFI_CONNECTED_BIT), US1 (display weather area) | `weather_task` waits on WIFI_CONNECTED_BIT |
| US5 (P5) | US1 (display), US3 (wifi state), US4 (weather freshness) | Status string requires all three state sources |
| US6 (P6) | All modules scaffolded (US1–US5) | Boundary enforcement pass after all modules exist |
| US7 (P7) | T024, T025 (time_format.c), T039 (weather_parser.c), T009 (app_config_validate) | Host tests target already-written source files |
| US8 (P8) | All modules complete | Docs require final API shapes to be accurate |

### Within Each User Story

- Headers (`*.h`) before implementations (`*.c`)
- `init()` before `start()` / task spawn implementation
- `CMakeLists.txt` creation [P] alongside implementation (different file)
- `main.c` wiring updates after the component under development is complete

### Parallel Opportunities

- All Phase 1 tasks T002–T007 are fully parallel (independent files)
- In Phase 2: `error_handler` (T015–T017) fully parallel with `app_state` (T012–T014)
- In Phase 3: `time_format.c` functions (T024, T025) fully parallel with `display.c` functions (T019–T021)
- In Phase 9 (US7): test files T055, T056, T057, T068 parallel once stubs (T053, T054) exist; T067 (extract `status_builder.c`) must precede T068
- Phases 8 (US6 audit) and 9 (US7 host tests) can proceed in parallel with each other
- All Phase 10 integration test stubs T062, T063, T064 are fully parallel

---

## Parallel Example: User Story 1

```bash
# These groups run concurrently (different files, no cross-dependencies):

# Group A — display component:
T019: display_init() in components/display/display.c
T020: display_render() in components/display/display.c
T021: display_show_message() in components/display/display.c
T022: components/display/CMakeLists.txt

# Group B — time_format (parallel with Group A):
T024: time_format_hms() in components/time_sync/time_format.c
T025: time_format_date() in components/time_sync/time_format.c
T026: components/time_sync/CMakeLists.txt

# Wait for T018-T026, then wire:
T027: main/main.c — app_main() + clock_task
```

---

## Parallel Example: User Story 7 (Host Tests)

```bash
# Write stubs first (T053, T054), then these run concurrently:
T055: tests/host/test_weather_parser.c
T056: tests/host/test_time_format.c
T057: tests/host/test_app_config.c
T067: main/status_builder.c + main/status_builder.h (extract from main.c)
T068: tests/host/test_status_builder.c
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks all stories)
3. Complete Phase 3: User Story 1 (display + clock_task)
4. **STOP and VALIDATE**: Flash device, confirm `HH:MM:SS` updates every second
5. Deliver MVP — a working embedded clock

### Incremental Delivery

1. Setup + Foundational → Compile baseline ready
2. US1 → Working clock display → **Demo / MVP**
3. US3 → Wi-Fi auto-recovery → Device self-heals after outages
4. US2 → NTP sync → Time is always accurate
5. US4 → Weather display → Full feature set
6. US5 → Status feedback → User-visible diagnostics
7. US6 → Architecture audit → Codebase is maintainable
8. US7 → Host tests → Regression protection without hardware
9. US8 → Documentation → Project is fully hand-off ready
10. Polish → Integration tests + full validation

### Recommended Solo-Developer Order

With a single developer, work US3 before US2 (Wi-Fi must exist before NTP task can wait on its event group):

> Phase 1 → Phase 2 → US1 → US3 → US2 → US4 → US5 → US6 → US7 → US8 → Polish

### Parallel Team Strategy

With three developers after Phase 2 completes:
- **Dev A**: US1 (display + clock_task) → US5 (status strings)
- **Dev B**: US3 (wifi_manager) → US2 (time_sync full) → US4 (weather_service)
- **Dev C**: US7 (host tests, parallel once source files exist) → US8 (documentation)

---

## Notes

- `[P]` tasks are in different files with no dependencies on other in-progress tasks in the same phase
- `[USn]` label maps each task to its user story for traceability and independent testing
- Each user story phase ends with an independent, hardware-verifiable checkpoint
- All host tests (T055–T057, T068) MUST pass before flashing any firmware
- `secrets.h` MUST be gitignored before the first commit containing config — see T006
- Avoid: vague tasks, same-file conflicts within a parallel group, cross-story dependencies that break independent testability
- Commit after each task or logical group; use the task ID (e.g., `T027`) in the commit message for traceability
