# Implementation Plan: Kronos MVP — Embedded IoT Digital Clock with Weather Display

**Branch**: `001-mvp-clock` | **Date**: 2026-08-03 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/001-mvp-clock/spec.md`

## Summary

Build ESP32-WROOM-32 firmware that displays HH:MM:SS local time (NTP-synced, RTC-fallback)
and an hourly weather forecast (next 3–6 hours) on an I2C OLED display. Seven distinct
device-status states are surfaced at all times. Wi-Fi reconnects with exponential back-off;
weather and NTP run on separate FreeRTOS tasks so clock display never blocks. Firmware is
structured as eight discrete ESP-IDF components with host-testable business logic.

## Technical Context

**Language/Version**: C17 (primary), C++17 permitted for RAII where beneficial; ESP-IDF
v5.x (latest stable at project inception, version pinned in `idf_component.yml`)

**Primary Dependencies**: ESP-IDF built-ins — `esp_wifi`, `esp_sntp`, `esp_http_client`,
`esp-cjson`; FreeRTOS (included); `esp-idf-ssd1306` (nopnop2002, via IDF component
manager) for OLED driver; Unity (built into ESP-IDF) for host and on-device tests

**Storage**: ESP32 NVS (Non-Volatile Storage) for runtime config and secrets; no external
database; `sdkconfig.local` + `secrets.h` (gitignored) for build-time provisioning

**Testing**: Unity host-compiled tests (linux target via `idf.py -T`) for weather parsing,
time formatting, config validation, and display data preparation; ESP-IDF component tests
(`idf.py -T`) for on-device integration paths

**Target Platform**: ESP32-WROOM-32 — Xtensa LX6 dual-core 240 MHz, 520 KB SRAM, 4 MB
flash; OLED SSD1306/SH1106 over I2C as reference display

**Project Type**: Embedded firmware (single-device, continuously running)

**Performance Goals**: Time display updates every second with no visible freeze; weather
fetch non-blocking; display refresh flicker-free via partial redraw; startup screen visible
within 2 seconds of power-on

**Constraints**: Total SRAM budget <520 KB (HTTPS TLS + JSON parse buffers evaluated
early); flash budget <4 MB including dual OTA partitions; HTTPS only; no heap allocation
in hot display path; all network ops have explicit timeouts; no blocking on main/display task

**Scale/Scope**: Single device, MVP feature set — 8 firmware modules, ~8 user stories,
full documentation set (architecture, modules, testing, deployment)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Verify the following before proceeding (Kronos Constitution v1.0.0):

- [x] **I. Embedded-First** — Static/NVS allocation; no dynamic alloc in hot paths; cJSON
  heap use bounded by JSON buffer size (evaluated in research.md §Memory); FreeRTOS tasks
  with fixed stack sizes; no large library deps beyond ESP-IDF built-ins + one display driver.
- [x] **II. Reliable Timekeeping** — NTP via `esp_sntp`; ESP32 RTC maintains time during
  network loss; TZ via POSIX TZ string in config; "time not synced" display state explicit.
- [x] **III. Weather API** — All HTTP calls isolated in `weather_service` component;
  parser in `weather_parser.c` (host-testable); stale-data retention, HTTP error handling,
  and parse-failure guard all specified in FR-013/014/015.
- [x] **IV. Wi-Fi & Network Resilience** — `wifi_manager` uses FreeRTOS event group and
  exponential back-off; NTP + weather run on dedicated tasks; display task never blocks on
  network; explicit timeouts on all outbound requests (FR-012).
- [x] **V. Display Abstraction** — `display` component owns all rendering; no other
  component writes to display hardware; accepts `DisplayPayload` struct (FR-019).
- [x] **VI. Module Boundaries** — Eight mandatory modules per constitution; acyclic
  dependency graph enforced by design; each module exposes minimal public `.h` header.
- [x] **VII. Security** — HTTPS enforced via `esp_http_client` + CA bundle (FR-007);
  credentials in `secrets.h` (gitignored) + NVS; `secrets.h.example` committed (FR-020);
  no plain HTTP permitted.
- [x] **VIII. Observability** — FR-018 mandates log coverage for all significant events;
  `ESP_LOGI/W/E/D` with per-module component tags; configurable log level via sdkconfig
  (FR-022).
- [x] **IX. Testability** — `weather_parser.c` and time-formatting logic have no ESP32
  platform deps; host-compiled Unity tests via `linux` target; platform-dependent code
  isolated behind thin adapters (FR per US-7).
- [x] **X. C/C++ Style** — Function ≤100 lines (target ≤50); `ESP_ERROR_CHECK` or
  explicit `if (ret != ESP_OK)` on all IDF API calls; `snake_case`; no magic numbers.
- [x] **XI. User Experience** — Seven distinct status states mandated (SC-005, FR-001,
  FR-006); startup screen within 2 seconds (SC-001 / Principle XI); time always visible.
- [x] **XII. Extensibility** — Dual OTA partitions in `partition-table.csv` from day one;
  `input_manager` extension point noted in architecture doc; no speculative abstractions.

## Project Structure

### Documentation (this feature)

```text
specs/001-mvp-clock/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
main/
├── main.c                       # app_main: init all modules, start FreeRTOS tasks
└── CMakeLists.txt

components/
├── wifi_manager/                # Wi-Fi connect, reconnect, exponential back-off
│   ├── include/wifi_manager.h
│   ├── wifi_manager.c
│   └── CMakeLists.txt
├── time_sync/                   # SNTP init, RTC fallback, POSIX TZ, time formatting
│   ├── include/time_sync.h
│   ├── time_sync.c
│   ├── time_format.c            # host-testable: no ESP32 platform deps
│   └── CMakeLists.txt
├── weather_service/             # HTTP fetch, cJSON parse, staleness logic
│   ├── include/weather_service.h
│   ├── weather_service.c
│   ├── weather_parser.c         # host-testable: no ESP32 platform deps
│   └── CMakeLists.txt
├── display/                     # Display abstraction + SSD1306 reference driver
│   ├── include/display.h
│   ├── display.c                # accepts DisplayPayload, calls driver
│   ├── drivers/
│   │   └── ssd1306/             # esp-idf-ssd1306 component (nopnop2002)
│   └── CMakeLists.txt
├── app_config/                  # NVS read/write, sdkconfig.local bridge, validation
│   ├── include/app_config.h
│   ├── app_config.c
│   └── CMakeLists.txt
├── app_state/                   # Shared runtime state protected by FreeRTOS mutex
│   ├── include/app_state.h
│   ├── app_state.c
│   └── CMakeLists.txt
└── error_handler/               # ESP_LOGE wrappers, safe-restart policy
    ├── include/error_handler.h
    ├── error_handler.c
    └── CMakeLists.txt

tests/
├── host/                        # Host-compiled Unity tests (weather parser, time fmt)
│   ├── test_weather_parser.c
│   ├── test_time_format.c
│   ├── test_app_config.c
│   └── CMakeLists.txt
└── integration/                 # On-device idf.py -T tests
    ├── test_wifi_manager/
    ├── test_time_sync/
    └── test_weather_service/

docs/
├── architecture.md              # Stack, module map, dependency graph
├── modules.md                   # Per-module purpose, public API, FreeRTOS tasks
├── testing.md                   # Host test + on-device test step-by-step
└── deployment.md                # Build, flash, monitor, update config

partition-table.csv              # app0, app1 (OTA), nvs, phy_init
secrets.h.example                # Placeholder template — committed
sdkconfig.defaults               # Shared build defaults — committed
.gitignore                       # secrets.h, sdkconfig.local excluded
idf_component.yml                # ESP-IDF version pin + external components
```

**Structure Decision**: Single ESP-IDF project with all logic in `components/` directory.
No separate frontend/backend split. Eight components map 1:1 to constitution-mandated
modules. `tests/host/` is a standalone CMake project that links only the platform-agnostic
source files, enabling `cmake -B build && cmake --build build && ./build/host_tests` on
any Linux/macOS/Windows host without ESP-IDF.

## Complexity Tracking

No constitution violations. No extra projects or patterns beyond what the MVP requires.
