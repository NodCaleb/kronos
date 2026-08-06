# Testing Guide: Kronos MVP

**Version**: 0.1.0

Two test tiers: **host unit tests** (no hardware, runs on any dev machine) and
**on-device integration tests** (requires ESP32 + USB cable).

All host tests MUST pass before flashing any firmware.

---

## Prerequisites

### Host Tests

- `cmake` ≥ 3.16 on PATH
- A C compiler on PATH (`gcc` or `clang` on Linux/macOS; `cl.exe` or `gcc` via MinGW/MSYS2
  on Windows)

### On-Device Tests

- ESP-IDF v5.x installed (`idf.py` on PATH)
- ESP32-WROOM-32 connected via USB
- Serial port identified (e.g., `/dev/ttyUSB0` on Linux, `COM3` on Windows)

---

## Host Unit Tests

Host tests compile and run on the development machine with no ESP32 required.
They cover the four host-testable modules: `weather_parser`, `time_format`,
`app_config_validate`, and `build_status_string`.

### Setup and Run

```bash
# From repo root
cd tests/host
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Linux / macOS
./build/host_tests

# Windows
.\build\host_tests.exe
```

### Expected Output

All tests must report `PASS`. Example output:

```
tests/host/test_weather_parser.c:42:TEST(WeatherParser, ParsesCurrentTemp):PASS
tests/host/test_weather_parser.c:51:TEST(WeatherParser, RejectsEmptyHourlyArray):PASS
tests/host/test_weather_parser.c:60:TEST(WeatherParser, RejectsMissingCurrent):PASS
tests/host/test_weather_parser.c:70:TEST(WeatherParser, RetainsOldDataOnFailure):PASS
tests/host/test_weather_parser.c:80:TEST(WeatherParser, RejectsZeroConditionId):PASS
tests/host/test_time_format.c:25:TEST(TimeFormat, FormatsMidnight):PASS
tests/host/test_time_format.c:34:TEST(TimeFormat, FormatsNoon):PASS
tests/host/test_time_format.c:43:TEST(TimeFormat, Formats235959):PASS
tests/host/test_time_format.c:52:TEST(TimeFormat, FormatsDate):PASS
tests/host/test_app_config.c:28:TEST(AppConfig, RejectsEmptySsid):PASS
tests/host/test_app_config.c:37:TEST(AppConfig, RejectsHttpUrl):PASS
tests/host/test_app_config.c:46:TEST(AppConfig, RejectsRefreshTooLow):PASS
tests/host/test_app_config.c:55:TEST(AppConfig, RejectsRefreshTooHigh):PASS
tests/host/test_app_config.c:64:TEST(AppConfig, RejectsTimeoutTooHigh):PASS
tests/host/test_app_config.c:73:TEST(AppConfig, RejectsRetryIntervalTooHigh):PASS
tests/host/test_app_config.c:82:TEST(AppConfig, AcceptsValidConfig):PASS
tests/host/test_status_builder.c:30:TEST(StatusBuilder, WifiConnecting):PASS
tests/host/test_status_builder.c:39:TEST(StatusBuilder, TimeSyncing):PASS
tests/host/test_status_builder.c:48:TEST(StatusBuilder, Synced):PASS
tests/host/test_status_builder.c:57:TEST(StatusBuilder, WeatherStale):PASS
tests/host/test_status_builder.c:66:TEST(StatusBuilder, WeatherUnavailable):PASS
tests/host/test_status_builder.c:75:TEST(StatusBuilder, WifiOfflineWithSync):PASS
tests/host/test_status_builder.c:84:TEST(StatusBuilder, OfflineNoSync):PASS

-----------------------
23 Tests 0 Failures 0 Ignored
OK
```

### Host Test Source Layout

```
tests/host/
├── CMakeLists.txt          # Standalone CMake project; links platform-agnostic .c files
├── CMakePresets.json
├── host_stubs.h            # Minimal stubs: esp_err_t, ESP_OK, ESP_LOGI/W/E no-ops
├── test_main.c             # Unity runner: calls all test suite runners
├── test_weather_parser.c   # Tests for weather_parse()
├── test_time_format.c      # Tests for time_format_hms() / time_format_date()
├── test_app_config.c       # Tests for app_config_validate()
└── test_status_builder.c   # Tests for build_status_string() — all 7 status states
```

The CMake project links only these platform-agnostic source files (no ESP-IDF headers):

- `components/weather_service/weather_parser.c`
- `components/time_sync/time_format.c`
- `components/app_config/app_config.c` (validation path only)
- `main/status_builder.c`

Platform guards (`#ifdef HOST_BUILD`) prevent any ESP32-specific code from being compiled
on the host. The `HOST_BUILD=1` preprocessor definition is set by `CMakeLists.txt`.

---

## On-Device Integration Tests

Integration tests run directly on the ESP32 and verify platform-dependent init paths.

### Running Tests

```bash
# Run each test project individually (ESP-IDF component test runner)
idf.py -T tests/integration/test_wifi_manager   -p /dev/ttyUSB0
idf.py -T tests/integration/test_time_sync      -p /dev/ttyUSB0
idf.py -T tests/integration/test_weather_service -p /dev/ttyUSB0
idf.py -T tests/integration/test_display         -p /dev/ttyUSB0
idf.py -T tests/integration/test_app_state       -p /dev/ttyUSB0
idf.py -T tests/integration/test_error_handler   -p /dev/ttyUSB0
```

Replace `/dev/ttyUSB0` with the correct serial port for your system.

### Expected Serial Output

Each test run ends with a Unity summary line in the serial monitor:

```
-----------------------
N Tests 0 Failures 0 Ignored
OK
```

### What Each Integration Test Verifies

| Test project | Verifies |
|---|---|
| `test_wifi_manager` | `wifi_manager_init()` returns `ESP_OK` for a valid `device_config_t` |
| `test_time_sync` | `time_sync_init()` returns `ESP_OK`; initial state is `TIME_STATE_NOT_SYNCED` |
| `test_weather_service` | `weather_service_init()` returns `ESP_OK`; initial `freshness` is `WEATHER_UNAVAILABLE` |
| `test_display` | `display_init()` returns `ESP_OK`; splash screen appears; no crash |
| `test_app_state` | `app_state_init()` returns `ESP_OK`; `try_read` returns zero-initialised state |
| `test_error_handler` | `error_handler_warn()` completes without triggering `esp_restart()` |

---

## Validation Checklist

Run through these before considering the firmware release-ready:

- [ ] All host unit tests pass (`host_tests` returns `OK`)
- [ ] Firmware builds without warnings (`idf.py build`)
- [ ] Firmware boots and displays correct time (quickstart Scenario 2)
- [ ] Time updates every second; no freeze during weather fetch (Scenario 3)
- [ ] Wi-Fi outage recovery works without reboot (Scenario 4)
- [ ] Invalid API key shows `"Weather N/A"` (Scenario 5)
- [ ] All seven status states display correct footer text (Scenario 6)
- [ ] All on-device integration tests pass (Scenario 7)
- [ ] `git grep -rn "wifi_password\|api_key" -- '*.c' '*.h'` returns no credential values

See [quickstart.md](../specs/001-mvp-clock/quickstart.md) for step-by-step instructions
for each scenario.
