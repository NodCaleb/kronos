# Feature Specification: Kronos MVP — Embedded IoT Digital Clock with Weather Display

**Feature Branch**: `001-mvp-clock`

**Created**: 2026-07-01

**Status**: Draft

---

## Clarifications

### Session 2026-07-04

- Q: How frequently should the time display update — per second (HH:MM:SS) or per minute (HH:MM)? → A: Every second; display shows HH:MM:SS format
- Q: What is the expected weather forecast granularity — hourly slots, daily summaries, or a single next-period label? → A: Hourly; next 3–6 hours, each slot showing temperature and condition
- Q: What Wi-Fi retry strategy should the firmware use — fixed interval, exponential back-off with a cap, or uncapped exponential? → A: Exponential back-off with a configurable maximum interval cap
- Q: What does the “language/locale” config attribute govern — API response language, UI strings, both, or neither? → A: Neither for MVP; locale configuration is out of scope. Weather condition display will be icon-based (not text) in a future iteration; condition codes must be preserved in the data model now to enable that.
- Q: What is the weather data staleness threshold — a fixed default, derived from the refresh interval, or independently configurable? → A: Derived; data is stale when its age exceeds 2× the configured refresh interval (no separate threshold config)

---

## User Scenarios & Testing *(mandatory)*

<!--
  User stories are prioritized as independent user journeys ordered by importance.
  P1 = most critical to MVP viability. Each story is independently testable.
-->

### User Story 1 — Reliable Local Time Display (Priority: P1)

As a user, I want the clock to show the correct local time at all times so that I can use it
as a reliable desk or wall clock.

**Why this priority**: Time display is the primary function of the device. If nothing else works,
this must work. All other features are secondary to a correct, continuously updating clock.

**Independent Test**: Can be fully tested by powering on a device with valid Wi-Fi and NTP
configuration and observing that the display shows the correct local time, updating at least
once per minute. Delivers core clock value independently of weather.

**Acceptance Scenarios**:

1. **Given** the device has valid Wi-Fi and NTP configuration, **When** the device completes
   startup, **Then** the display shows correct local time in the configured time zone and
   continues updating without interruption.

2. **Given** the device has previously synchronized time and Wi-Fi becomes unavailable,
   **When** the network drops, **Then** the display continues showing time based on the
   internal system clock without freezing or displaying an error on the time field.

3. **Given** weather data fetching is in progress or fails, **When** the display refreshes,
   **Then** the time field continues updating and the clock does not freeze or go blank.

---

### User Story 2 — Automatic Internet Time Synchronization (Priority: P2)

As a user, I want the clock to automatically synchronize time from the internet so that I
never need to set the time manually.

**Why this priority**: Manual time setting is impractical on an embedded device with no
physical controls. Automatic synchronization is what makes the device "set and forget".

**Independent Test**: Can be fully tested by observing the device connect to Wi-Fi,
synchronize with an NTP source, and display correct local time with a "synchronized" status
indicator. Verifiable by comparing displayed time to a reference clock.

**Acceptance Scenarios**:

1. **Given** Wi-Fi is connected, **When** NTP synchronization completes, **Then** the display
   shows correct local time in the configured time zone and logs a successful sync event.

2. **Given** Wi-Fi is connected but NTP synchronization fails, **When** the device attempts
   to synchronize, **Then** the display indicates that time is not yet synchronized, the
   device retries later, and it does not crash or freeze.

3. **Given** the device has synchronized and then loses network access, **When** network
   access is restored, **Then** the device automatically re-synchronizes time without user
   intervention.

---

### User Story 3 — Wi-Fi Connectivity with Automatic Recovery (Priority: P3)

As a user, I want the device to connect to my Wi-Fi network automatically and recover after
outages so that I do not need to manually intervene.

**Why this priority**: Wi-Fi connectivity enables both NTP synchronization and weather
fetching. Automatic recovery means the device is self-healing after router restarts or
temporary signal loss.

**Independent Test**: Can be fully tested by observing the device boot with an unavailable
network, display an offline/retrying status, and then automatically reconnect and resume
full operation once the network becomes available.

**Acceptance Scenarios**:

1. **Given** the configured Wi-Fi network is available, **When** the device boots, **Then**
   the display shows a connecting status followed by a connected status and the device
   proceeds to time synchronization.

2. **Given** the configured Wi-Fi network is unavailable during boot, **When** the device
   starts, **Then** the display shows an offline or retrying status, the device continues
   retrying, and it does not crash or enter a reboot loop.

3. **Given** the device is connected and Wi-Fi is interrupted, **When** connectivity is
   restored, **Then** the device automatically reconnects without user intervention and
   resumes weather and NTP operations.

---

### User Story 4 — Current Weather and Short Forecast Display (Priority: P4)

As a user, I want to see current weather conditions and a short forecast on the same display
so that I can quickly understand outdoor conditions without checking a separate device.

**Why this priority**: Weather display is the distinguishing value-add of this device over a
plain digital clock. It is secondary to the clock function and must never compromise it.

**Independent Test**: Can be fully tested by observing the device fetch and display current
temperature, weather condition, and a short forecast (today/tomorrow or next few hours) on
the display after a successful API response.

**Acceptance Scenarios**:

1. **Given** Wi-Fi is connected and weather API configuration is valid, **When** the device
   requests weather data, **Then** the display shows current temperature, weather condition,
   and short forecast information.

2. **Given** the device has previously shown weather data, **When** a subsequent refresh
   fails, **Then** the display continues showing the last valid weather data marked as stale,
   and the time display is unaffected.

3. **Given** the device has no prior weather data, **When** weather fetching fails, **Then**
   the display shows a clear fallback message in the weather area and the clock continues
   functioning.

4. **Given** the weather API returns malformed or incomplete data, **When** the firmware
   processes the response, **Then** it rejects the invalid data, logs a parsing error, and
   does not overwrite the last valid weather data.

---

### User Story 5 — Clear Device Status Feedback (Priority: P5)

As a user, I want the display to communicate what the device is currently doing so that I
can tell at a glance whether it is working correctly or experiencing a problem.

**Why this priority**: Without visible status indicators, users cannot distinguish between
"working normally", "waiting for Wi-Fi", "weather is stale", and "something is broken".
Clear feedback reduces confusion and support burden.

**Independent Test**: Can be fully tested by cycling through known device states (booting,
connecting, synced, offline, stale weather) and verifying that each state produces a
distinct, human-readable indicator on the display.

**Acceptance Scenarios**:

1. **Given** the device is booting, **When** startup initialization is in progress, **Then**
   the display shows startup progress messages within 2 seconds of power-on.

2. **Given** the device is in a specific operational state, **When** the display refreshes,
   **Then** the status area shows a distinct indicator for: connecting to Wi-Fi, Wi-Fi
   connected, Wi-Fi offline, time synchronized, time not synchronized, weather fresh,
   weather stale, and weather unavailable.

---

### User Story 6 — Modular, Maintainable Firmware Architecture (Priority: P6)

As a developer, I want Wi-Fi, time synchronization, weather fetching, configuration, and
display rendering separated into discrete modules so that the firmware is maintainable,
reviewable, and each module can be modified without breaking others.

**Why this priority**: A tightly coupled monolith would make every future change risky and
expensive. Module boundaries enforced at MVP establish the architectural foundation for
all future development.

**Independent Test**: Can be fully tested by code review confirming that each module has a
defined public interface, no module directly accesses another module's internals, and the
display module accepts structured data rather than fetching it directly.

**Acceptance Scenarios**:

1. **Given** the firmware is built and deployed, **When** a developer reviews the source,
   **Then** distinct modules for Wi-Fi management, time synchronization, weather fetching,
   weather parsing, display rendering, configuration, application state, and diagnostics
   are identifiable with clear public headers.

2. **Given** the display module is reviewed, **When** its implementation is examined, **Then**
   it contains no direct calls to network APIs, NTP functions, or weather API endpoints.

---

### User Story 7 — Host-Testable Business Logic (Priority: P7)

As a developer, I want weather parsing and display data preparation to be testable on a
development machine without requiring physical ESP32 hardware so that bugs can be caught
faster and the test cycle is shorter.

**Why this priority**: Deploying firmware to hardware for every test of a JSON parser or
time formatter is slow and wasteful. Host-testable logic enables rapid iteration.

**Independent Test**: Can be fully tested by running a host-compiled test suite that
exercises weather JSON parsing, time formatting, and display data preparation functions
using known input/output pairs without any hardware present.

**Acceptance Scenarios**:

1. **Given** a known weather API response payload, **When** the parser processes it on
   the host, **Then** it produces the correct structured weather data output.

2. **Given** a timestamp and time zone configuration, **When** the time formatting function
   runs on the host, **Then** it produces the correct local time string.

---

### User Story 8 — Comprehensive Project Documentation (Priority: P8)

As a developer, I want the project to include written documentation covering architecture,
module responsibilities, testing procedures, and deployment steps so that any developer
familiar with C/C++ and ESP-IDF can understand, build, test, and update the firmware
without prior project knowledge.

**Why this priority**: Undocumented embedded projects are difficult to hand off, revisit
after a break, or contribute to. Clear documentation reduces onboarding time and lowers
the risk of misconfiguration during deployment or testing.

**Independent Test**: Can be fully tested by giving the documentation to a developer
unfamiliar with the project and verifying they can: understand the module structure from
the architecture description, identify the purpose of each module, flash and test the
firmware using only the documented steps, and apply a configuration update using the
deployment guide — without asking clarifying questions.

**Acceptance Scenarios**:

1. **Given** the project documentation exists, **When** a developer reads the architecture
   description, **Then** they can identify the technology stack, list all project modules,
   and describe the responsibility of each module and how they interact.

2. **Given** the project documentation exists, **When** a developer reads the module
   descriptions, **Then** each module's purpose, public interface, and key behaviors are
   described clearly enough to use or modify the module without reading its full
   implementation.

3. **Given** the project documentation exists, **When** a developer follows the testing
   manual, **Then** they can run both the host-compiled unit tests and the on-device
   integration tests successfully using only the documented steps.

4. **Given** the project documentation exists, **When** a developer follows the deployment
   manual, **Then** they can build, flash, and monitor the firmware on a target device, and
   apply a configuration change (e.g., update Wi-Fi credentials or weather API key) using
   only the documented steps.

---

### Edge Cases

- What happens when the configured Wi-Fi SSID does not exist or the password is wrong?
  The device must display a connection failure status and keep retrying without crashing.

- What happens when the NTP server is unreachable after Wi-Fi connects?
  The device must display "time not synchronized", keep retrying NTP, and not block display
  updates.

- What happens when the weather API key is invalid or expired?
  The device must handle the HTTP error response gracefully, log the error, and display
  weather as unavailable without crashing.

- What happens when the weather API returns HTTP 429 (rate limited)?
  The device must treat this as a retriable failure, retain last valid data, and back off
  before retrying.

- What happens when the weather API response is valid JSON but missing expected fields?
  The parser must reject partial data, log the specific missing fields, and not overwrite
  the last valid weather data.

- What happens when the device clock drifts significantly before re-synchronization?
  After re-synchronization the display must immediately show the corrected time and log
  the drift event.

- What happens when the device runs continuously for days without NTP re-sync?
  The device must keep displaying the internally tracked time and clearly indicate how
  long since the last NTP sync if the threshold is configurable.

- What happens if flash or NVS storage for configuration is corrupted?
  The firmware must log the error and fail safely; it must not silently use invalid
  configuration values.

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The firmware MUST display startup progress on the screen during device
  initialization.
- **FR-002**: The firmware MUST connect to a configured Wi-Fi network using credentials
  stored outside of committed source code.
- **FR-003**: The firmware MUST automatically retry Wi-Fi connection after a connection
  failure using exponential back-off, with a configurable maximum retry interval cap
  to prevent indefinitely long gaps between attempts.
- **FR-004**: The firmware MUST synchronize current time using a network time source
  (NTP).
- **FR-005**: The firmware MUST apply a configurable time zone setting to all displayed
  times.
- **FR-006**: The firmware MUST display a clear indicator when time has not yet been
  synchronized.
- **FR-007**: The firmware MUST retrieve current weather data from a configurable external
  API endpoint using an encrypted connection (HTTPS).
- **FR-008**: The firmware MUST parse and store current weather conditions including at
  minimum: temperature, a weather condition description (text, in English), and the raw
  condition code from the API response. The condition code MUST be retained in the data
  model to enable future icon-based rendering without requiring a data model change.
- **FR-009**: The firmware MUST parse and display an hourly weather forecast covering the
  next 3–6 hours, with each time slot providing at minimum a temperature value and a
  weather condition description.
- **FR-010**: The firmware MUST refresh weather data on a configurable interval (default
  15–60 minutes).
- **FR-011**: The firmware MUST update the displayed time independently of and without
  blocking on weather refresh operations or any other network activity.
- **FR-012**: The firmware MUST apply configurable timeouts to all outbound network
  requests (NTP and weather API).
- **FR-013**: The firmware MUST handle failed HTTP requests gracefully without crashing
  or freezing.
- **FR-014**: The firmware MUST handle invalid, incomplete, or malformed API responses
  without crashing, and MUST NOT overwrite the last valid weather data on a parse failure.
- **FR-015**: The firmware MUST retain and display the last valid weather data when a
  refresh attempt fails, marking it visibly as stale.
- **FR-016**: The firmware MUST display a fallback message in the weather area when no
  weather data has ever been successfully retrieved.
- **FR-017**: The firmware MUST mark weather data as stale when its age exceeds twice
  the configured weather refresh interval. No separate staleness threshold configuration
  is required; the stale state is derived automatically from the refresh interval.
- **FR-018**: The firmware MUST emit diagnostic log messages to the serial output for
  the following events: device boot, Wi-Fi connection attempt and outcome, NTP
  synchronization attempt and outcome, weather API request and outcome, JSON parse errors,
  display update events, and all error conditions.
- **FR-019**: The firmware MUST isolate all display rendering behind a dedicated display
  module; no other module MAY write to the display hardware directly.
- **FR-020**: The firmware MUST store Wi-Fi credentials and API keys in a location that is
  not committed to version control.
- **FR-021**: The firmware MUST automatically recover and resume normal operation when
  Wi-Fi connectivity is restored after an outage.
- **FR-022**: The firmware MUST provide a configurable debug logging level that can be
  adjusted without changing business logic code.
- **FR-023**: The project MUST include an architecture document describing the technology
  stack, all modules, each module's responsibility, and the dependencies between modules.
- **FR-024**: Each module MUST be described in documentation covering its purpose, public
  interface, and key runtime behaviors.
- **FR-025**: The project MUST include a testing manual with step-by-step instructions for
  running host-compiled unit tests and on-device integration tests.
- **FR-026**: The project MUST include a deployment manual with step-by-step instructions
  for building, flashing, monitoring the firmware, and updating configuration values such
  as Wi-Fi credentials and the weather API key.

### Key Entities

- **WeatherData**: Represents the most recently fetched weather information. Key attributes:
  current temperature, weather condition description (English text), weather condition code
  (raw API identifier preserved for future icon-based rendering), hourly forecast entries
  (a list of 3–6 slots each containing a time label, temperature, condition description,
  and condition code), data fetch timestamp, and a freshness flag (fresh / stale /
  unavailable).

- **AppState**: Represents the current runtime state of the device. Key attributes:
  Wi-Fi connection status, time synchronization status, current local time value,
  weather availability status, timestamp of last successful weather update, and a
  human-readable status message for display.

- **DeviceConfig**: Represents all user-configurable device settings. Key attributes:
  Wi-Fi SSID and credentials reference, weather API endpoint URL, API key reference,
  location identifier, measurement units, time zone (POSIX TZ string), weather refresh
  interval, request timeout value, and debug log level. The weather data staleness
  threshold is derived as 2× the refresh interval and is not a separate config value.

- **DisplayPayload**: Represents the structured data prepared for rendering on the display.
  Key attributes: formatted current time string in HH:MM:SS format (updated every second),
  formatted date string, current weather summary line, one or more compact hourly forecast
  lines (time + temperature + condition, truncated to fit the display), and a
  status/indicator string. No raw network data or configuration values should appear in
  this entity.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The display shows the correct local time within 30 seconds of completing
  startup, assuming the configured Wi-Fi network and NTP source are available.

- **SC-002**: The time display updates at least once per second (displaying time in
  HH:MM:SS format) with no visible freeze, regardless of whether a weather refresh is
  in progress or has failed.

- **SC-003**: When a weather refresh fails, the clock continues updating uninterrupted;
  the weather failure must not cause any visible delay or freeze in the time display.

- **SC-004**: After a Wi-Fi outage ends, the device automatically resumes NTP
  synchronization and weather updates within 5 minutes, with no user intervention.

- **SC-005**: The display surfaces at least seven distinct human-readable status states:
  Wi-Fi connecting, Wi-Fi connected, Wi-Fi offline, time synchronized, time not
  synchronized, weather fresh, weather stale, and weather unavailable.

- **SC-006**: The device does not reboot, crash, or freeze when the configured Wi-Fi
  network is unavailable at startup or during operation.

- **SC-007**: Under normal operating conditions with a working network, weather data is
  refreshed at least once per hour.

- **SC-008**: No Wi-Fi password or weather API key appears as a plaintext constant in any
  file committed to the project's version control repository.

- **SC-009**: Weather JSON parsing and display data preparation functions produce correct
  output when exercised with known test inputs on a host machine, independently of any
  physical hardware.

---

## Assumptions

- A single Wi-Fi network profile is configured; multi-network roaming or fallback profiles
  are out of scope for MVP.
- A single weather data provider is used for MVP; the architecture must permit future
  provider substitution without changes to display or application state modules.
- The display is a small monochrome module (I2C OLED, e.g. SSD1306 or SH1106 class);
  the display module abstraction must not assume a specific driver, enabling future
  replacement.
- The weather API provides both current conditions and a short forecast in a single
  JSON-based HTTPS response. The exact API provider is chosen during implementation;
  OpenWeatherMap or a compatible service is the reference.
- The device operates at a fixed geographic location; dynamic location detection or
  GPS integration is out of scope.
- No physical controls (buttons, rotary encoder) are required for MVP unless needed
  to support basic testing or a factory reset.
- The firmware targets an ESP32-WROOM-32 module with 520 KB SRAM and 4 MB flash;
  memory usage must be evaluated against this constraint before adoption of any new
  dependency.
- The partition table must allocate dual OTA partitions from the start to support future
  over-the-air update capability, even though OTA logic is not implemented in MVP.
- Development toolchain is VS Code with the Espressif IDF extension; build, flash, and
  monitor operations use standard IDF commands.
- Internet NTP pool (e.g., pool.ntp.org) is the default time source; the NTP server
  address must be configurable.
- Weather condition text will be displayed in English for MVP. A future iteration will
  replace text labels with icons; the WeatherData condition code field is preserved now
  to support that without a data model migration.

---

## Non-Goals *(MVP Scope Boundary)*

The following are explicitly out of scope for this MVP:

- Mobile application companion or remote control interface
- Bluetooth-based device configuration
- Web-hosted configuration portal
- Over-the-air (OTA) firmware update logic (partition table must accommodate it, but no
  OTA code will be implemented)
- Alarm, timer, or notification functionality
- Physical controls (buttons, touch, rotary encoder) beyond what is strictly needed for
  basic testing
- Battery operation or power optimization
- Animated, color, or multi-page display layouts
- Support for multiple concurrent weather providers or switchable providers at runtime
- Indoor environmental sensors (temperature, humidity, air quality)
- Multi-timezone or multi-location support
- Persistent storage of historical weather data or trends
- Language/locale configuration and weather condition text localization (condition text
  is English only for MVP; icon-based display without text is a post-MVP feature)

---

## Risks

- **Weather API availability and terms**: Free-tier weather API quotas may be exceeded
  during development; rate limiting responses must be handled gracefully. Mitigation:
  configurable refresh interval and back-off on 429 responses (covered by FR-013).

- **ESP32 memory pressure**: HTTPS TLS connections and JSON parsing buffers both consume
  significant heap on a resource-constrained device. Mitigation: evaluate heap usage early
  and choose a parser that supports streaming or fixed-size buffers.

- **NTP reliability**: The default NTP pool may be unreachable in some network
  environments. Mitigation: configurable NTP server address; RTC-based timekeeping after
  first sync minimizes re-sync dependency (Principle II).

- **Display driver availability and compatibility**: The OLED display driver must integrate
  with the ESP-IDF component system and fit within flash and RAM budgets. Mitigation:
  isolate the driver behind the display module abstraction so it can be swapped if a
  specific driver has issues (Principle V).

- **Time zone database complexity**: POSIX TZ strings cover most common zones but can be
  tricky for DST edge cases. Mitigation: test with known DST transition dates; document
  the TZ string format requirement for configuration.

- **Wi-Fi reconnect loops**: Aggressive reconnect retries could waste power and potentially
  affect device stability. Mitigation: exponential back-off with a configurable cap
  (FR-003, Principle IV) prevents both reconnect storms and excessively long retry gaps.

- **Configuration management on-device**: If NVS partition for secrets is not properly
  initialized, the device may boot with empty credentials. Mitigation: validate
  configuration at startup and surface clear error messages before attempting network
  operations.

---

## Embedded & Security Constraints *(Kronos Constitution)*

<!--
  Verify these constraints are addressed in requirements above.
  See .specify/memory/constitution.md for full principle definitions.
-->

- [ ] Flash/RAM impact assessed (Principle I — Embedded-First): FR-002, FR-007, FR-010,
      FR-012 all reference constraints on resource usage and avoid unbounded allocations.
      Concrete assessment required at implementation planning stage.

- [ ] Network operations non-blocking with timeouts (Principle IV): FR-011 mandates
      independent time display updates; FR-012 mandates configurable timeouts on all
      network requests.

- [ ] HTTPS used for all external calls; no hardcoded secrets (Principle VII): FR-007
      mandates HTTPS for weather API; FR-020 mandates secrets outside version control;
      SC-008 provides a verifiable success criterion.

- [ ] Serial log coverage planned for new events (Principle VIII): FR-018 enumerates
      all mandatory log events; FR-022 requires configurable log level.

- [ ] Host-testable logic isolated from hardware drivers (Principle IX): FR-019 mandates
      display module isolation; US-7 (User Story 7) and SC-009 establish the host-testable
      requirement for parsing and data preparation logic.

- [ ] Display state covers all offline/stale/error cases (Principles III, XI): FR-006,
      FR-015, FR-016, FR-017 cover all weather and time status states; SC-005 requires at
      least seven distinct visible status indicators.
