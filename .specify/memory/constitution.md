<!--
SYNC IMPACT REPORT
==================
Version change: (new) → 1.0.0
Modified principles: N/A — initial ratification
Added sections: Core Principles (I–XII), Technology Stack, Development Workflow, Governance
Removed sections: N/A
Templates updated:
  ✅ .specify/templates/plan-template.md — Constitution Check gates updated
  ✅ .specify/templates/spec-template.md — Constraints section updated
  ✅ .specify/templates/tasks-template.md — Phase descriptions updated
Follow-up TODOs: None — all fields resolved.
-->

# Kronos Constitution

## Core Principles

### I. Embedded-First Design

The firmware MUST be optimized for the ESP32-WROOM-32's memory, CPU, flash, and network
constraints at all times.

- Dynamic heap allocation MUST be minimized; prefer static allocation or fixed-size pools.
- Blocking operations on the main task MUST be avoided; use FreeRTOS tasks, queues, and
  event groups for concurrency.
- Dependencies MUST be evaluated for flash and RAM footprint before adoption.
- Deterministic, predictable resource usage is preferred over convenience abstractions.

**Rationale**: The ESP32-WROOM-32 has 520 KB SRAM and 4 MB flash. Uncontrolled allocation,
large libraries, or blocking code leads to stack overflows, watchdog resets, and
unpredictable behavior in a continuously running embedded device.

### II. Reliable Timekeeping

Time synchronization MUST be explicit, robust, and resilient to network interruptions.

- The system MUST use SNTP (NTP) as the primary time source via ESP-IDF's `esp_sntp` component.
- The ESP32 internal RTC MUST be used to maintain timekeeping during temporary network loss.
- Time zone and DST rules MUST be configurable via POSIX TZ strings stored in project
  configuration; they MUST NOT be hardcoded.
- The system MUST log synchronization events (success, failure, drift) to the serial monitor.
- A clear "time not synchronized" state MUST be surfaced on the display during startup until
  an NTP sync is confirmed.

**Rationale**: Correct time display is the primary function of this device. Silent time drift
or displaying an unsynchronized time without indication degrades the user experience and trust.

### III. Weather API Integration

Weather provider logic MUST be isolated behind a clean abstraction layer.

- A dedicated `weather` module MUST own all API communication; no other module MAY call
  weather endpoints directly.
- API endpoint URL, API key, units, language, and location MUST be configurable and MUST NOT
  be hardcoded.
- The firmware MUST handle API failures, HTTP error codes, malformed JSON responses, rate
  limit responses, and network outages without crashing or freezing.
- The display MUST visually distinguish between fresh data, stale data (last known), and
  fully unavailable weather data using distinct UI states.
- Stale data MUST be retained in memory and shown with a staleness indicator rather than
  clearing the display on failure.

**Rationale**: Weather APIs are external dependencies outside project control. Defensive
handling prevents a transient API issue from degrading the primary clock function.

### IV. Wi-Fi and Network Resilience

Wi-Fi connectivity MUST be treated as an unreliable resource that can fail at any time.

- The Wi-Fi manager MUST implement automatic reconnect with configurable retry intervals
  and exponential back-off.
- All network operations (NTP sync, API requests) MUST have explicit timeouts; indefinite
  blocking on network I/O is prohibited.
- The main display update loop MUST NOT be blocked by Wi-Fi connection attempts, NTP
  requests, or weather API calls; these MUST run on separate FreeRTOS tasks.
- Offline behavior (time display with stale or no weather) MUST be designed and tested as
  a first-class operating mode, not a fallback afterthought.

**Rationale**: Embedded devices in home environments experience frequent Wi-Fi disruptions.
The device MUST remain functional and useful at all times regardless of network state.

### V. Display Abstraction

All display-rendering code MUST be isolated from business logic and data-fetching logic.

- A `display` module MUST own all hardware-specific rendering calls (I2C/SPI writes,
  framebuffer operations). No other module MAY write to display hardware directly.
- The display module MUST accept structured data (time, weather, status) and translate it
  to pixels; it MUST NOT fetch data itself.
- The architecture MUST accommodate future display types (OLED SSD1306, TFT ILI9341,
  LCD, LED matrix) through a display interface or driver abstraction without requiring
  changes to business logic modules.
- Display updates MUST minimize flicker; partial redraws SHOULD be preferred over full
  screen clears where the driver supports it.

**Rationale**: Display hardware selection may change. Coupling rendering to business logic
makes driver replacement a full codebase change rather than a module swap.

### VI. Clear Module Boundaries

The codebase MUST be organized into discrete, single-responsibility modules with minimal
cross-module coupling.

- Mandatory modules: `wifi_manager`, `time_sync`, `weather_service`, `display`,
  `app_config`, `app_state`, `error_handler`.
- Each module MUST expose a minimal public header (`.h`) that hides implementation details.
- Large monolithic source files (>500 lines) MUST be refactored before merging.
- Inter-module dependencies MUST flow in one direction; circular dependencies are prohibited.
- Public module APIs MUST be documented with a brief description of purpose, parameters,
  return values, and error conditions.

**Rationale**: Clear boundaries make individual modules replaceable, testable in isolation,
and understandable without reading the entire codebase.

### VII. Security and Configuration

Sensitive values and configuration MUST be managed securely and never committed to
version control as plaintext.

- All external API communication MUST use HTTPS (TLS); plain HTTP endpoints are prohibited.
  ESP-IDF `esp_http_client` with certificate bundle MUST be used for TLS verification.
- API keys, Wi-Fi credentials, and other secrets MUST be stored using ESP-IDF's NVS
  (Non-Volatile Storage) partition or a local `secrets.h` / `sdkconfig.local` file that
  is listed in `.gitignore`. They MUST NOT appear in committed source files.
- A `secrets.h.example` template (with placeholder values only) MUST be committed to
  document required configuration without exposing actual credentials.
- Development and production configuration values MUST be separable via `sdkconfig`
  overlays or compile-time flags.

**Rationale**: API keys committed to source control are a well-documented cause of
credential leakage and account compromise.

### VIII. Observability and Diagnostics

The firmware MUST emit structured, actionable serial logs for all significant lifecycle events.

- Log entries MUST use ESP-IDF log macros (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, `ESP_LOGD`)
  with a consistent component tag per module.
- The following events MUST always be logged at INFO level or higher: device boot, Wi-Fi
  connection attempts and outcomes, NTP sync attempts and outcomes, weather API requests
  and outcomes, display updates, and all error conditions.
- Debug-level logs (`ESP_LOGD`) MUST be controllable via `CONFIG_LOG_DEFAULT_LEVEL` in
  `sdkconfig` to avoid flooding the serial monitor in production builds.
- Log messages MUST include enough context (component, state, error code) to diagnose
  issues through `idf.py monitor` without attaching a debugger.

**Rationale**: The primary debugging interface for deployed embedded firmware is the serial
monitor. Poor logging makes field issues impossible to diagnose.

### IX. Testability

Business logic MUST be designed to be testable independently of ESP32 hardware wherever
practical.

- Platform-dependent code (GPIO, I2C, SPI, NVS, Wi-Fi drivers) MUST be isolated behind
  interfaces or thin adapter layers so that business logic can be compiled and tested on
  the host.
- The following components MUST be unit-testable on the host: weather response JSON
  parsing, time formatting and DST logic, configuration validation, and display data
  preparation.
- ESP-IDF's `components` test framework (`idf.py -T`) SHOULD be used for on-device
  integration tests where host testing is insufficient.
- New modules MUST include at least a basic test covering the primary happy path before
  being considered complete.

**Rationale**: Waiting for a flashed device to test parsing logic or formatting functions
wastes development time. Host-testable logic catches bugs faster and at lower cost.

### X. Maintainable C/C++ Style

Code MUST be clear, consistent, and follow ESP-IDF conventions throughout the codebase.

- Functions MUST be small and focused (target ≤50 lines per function; hard limit 100 lines).
- All errors returned by ESP-IDF APIs MUST be checked; use `ESP_ERROR_CHECK` or explicit
  `if (ret != ESP_OK)` handling. Silent error discard is prohibited.
- Global state MUST be minimized; where global state is required it MUST be encapsulated
  in a module-level struct and documented.
- Naming MUST follow ESP-IDF style: `snake_case` for functions and variables, `UPPER_CASE`
  for constants and macros, component-prefix for public symbols (e.g., `time_sync_init()`).
- Magic numbers and string literals MUST be replaced with named constants or `#define` macros.
- C++ SHOULD be used only where it provides clear value (e.g., RAII resource management);
  prefer C idioms otherwise to stay compatible with the ESP-IDF component ecosystem.

**Rationale**: Consistent, simple code reduces review time, onboarding time, and the
likelihood of subtle bugs in safety-relevant firmware logic.

### XI. User Experience

The device MUST provide a clear, predictable, and informative experience at all times.

- On power-on, the display MUST show a startup screen with device name, firmware version,
  and current connection/sync status within 2 seconds of boot.
- The time display MUST have priority: it MUST be visible and updating at all times, even
  when weather data is unavailable.
- The display MUST provide distinct, human-readable status indicators for: connecting to
  Wi-Fi, time synchronized, time not synchronized, weather data fresh, weather data stale,
  and weather data unavailable.
- Display rendering MUST be stable and flicker-free; partial redraws SHOULD be used for
  time digits that change every second.

**Rationale**: A clock that shows a blank screen or freezes during network reconnection is
not a functional product. Clear status feedback reduces user confusion and support burden.

### XII. Extensibility

The architecture MUST leave defined extension points for future features without
overcomplicating the initial implementation.

- New input peripherals (buttons, touch, rotary encoder) MUST be addable as new
  `input_manager` components without modifying core clock logic.
- The weather service abstraction MUST allow swapping or adding weather providers without
  changing the display or application state modules.
- OTA update capability MUST be considered in the partition table layout from the start
  (dual OTA partitions), even if OTA logic is not implemented in v1.
- Future extension points MUST be noted in code comments or ADRs; they MUST NOT introduce
  speculative complexity or unused abstractions in v1.

**Rationale**: Embedded product requirements expand over the product lifecycle. A minimal
but well-bounded architecture is cheaper to extend than a tightly coupled monolith.

## Technology Stack

**Microcontroller**: Espressif ESP32-WROOM-32 (Xtensa LX6 dual-core, 240 MHz, 520 KB SRAM,
4 MB flash)

**Framework**: ESP-IDF (latest stable release at project inception; version pinned in
`idf_component.yml`)

**Language**: C (primary); C++ permitted where RAII or type safety provides clear benefit

**IDE**: Visual Studio Code with the Espressif IDF Extension

**Build system**: CMake via `idf.py` (ESP-IDF standard toolchain)

**Time source**: SNTP via ESP-IDF `esp_sntp` component; NTP pool (`pool.ntp.org`) as default

**Weather API**: Configurable via `app_config`; OpenWeatherMap or compatible REST API over
HTTPS as the reference implementation

**Display**: Configurable driver module; OLED (SSD1306/SH1106 over I2C) as the reference
implementation

**Secrets management**: `sdkconfig.local` + NVS for runtime secrets; `secrets.h.example`
committed as documentation template

**Testing**: ESP-IDF component unit tests (`idf.py -T`) for on-device tests; host-compiled
unit tests using a lightweight test harness (Unity or similar) for platform-independent logic

## Development Workflow

**Branch strategy**: Feature branches off `main`; branch naming `###-short-description`;
PRs required for all merges to `main`.

**Constitution gate**: Every PR description MUST include a "Constitution Check" section
confirming compliance with all 12 principles relevant to the change.

**Code review**: At least one review approval required before merge; reviewer MUST verify
ESP-IDF error handling, log coverage, and module boundary compliance.

**Testing gate**: All existing unit tests MUST pass (`idf.py -T`) before merge; new modules
MUST include tests for primary happy-path behavior.

**Security gate**: PRs MUST NOT introduce hardcoded credentials, plain HTTP API calls, or
new unchecked `esp_err_t` return values.

**Tooling**: `idf.py build`, `idf.py flash`, `idf.py monitor` are the standard build/deploy
commands. `idf.py size` MUST be reviewed when a PR changes flash or RAM footprint
significantly (>5% of available space).

**Documentation**: Each new module MUST include a header comment block describing its
purpose, public API, and any FreeRTOS tasks it creates.

## Governance

This constitution is the authoritative source of engineering principles for the Kronos
project. It supersedes all other documents, conventions, or verbal agreements where conflicts
arise.

**Amendment procedure**:
1. Propose the change in a PR targeting `.specify/memory/constitution.md`.
2. State the motivation, the principle(s) affected, and the version bump type (MAJOR/MINOR/PATCH).
3. Update all dependent templates (plan-template.md, spec-template.md, tasks-template.md)
   in the same PR.
4. Obtain at least one review approval before merging.

**Versioning policy**: Semantic versioning applies — MAJOR for backward-incompatible principle
removals or redefinitions; MINOR for new principles or materially expanded guidance; PATCH
for clarifications, wording, and non-semantic refinements.

**Compliance**: All contributors are responsible for constitution compliance in their own
PRs. Reviewers are responsible for flagging violations before approval. Unresolved compliance
disputes MUST be resolved before merge.

**Version**: 1.0.0 | **Ratified**: 2026-07-01 | **Last Amended**: 2026-07-01
