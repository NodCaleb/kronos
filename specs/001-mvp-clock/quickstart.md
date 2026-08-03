# Quickstart Validation Guide: Kronos MVP — 001-mvp-clock

**Branch**: `001-mvp-clock` | **Date**: 2026-08-03

This guide describes runnable validation scenarios that prove the feature works end-to-end.
For data structures see [data-model.md](data-model.md). For module APIs see
[contracts/module-apis.md](contracts/module-apis.md).

---

## Prerequisites

### Hardware

- ESP32-WROOM-32 development board (e.g., ESP32 DevKitC)
- I2C OLED display (SSD1306 or SH1106, 128×64)
  - SDA → GPIO 21, SCL → GPIO 22 (configurable via `sdkconfig`)
- USB cable for flashing and serial monitor
- Wi-Fi network with internet access

### Software

- ESP-IDF v5.x installed and `idf.py` on PATH
- VS Code + Espressif IDF extension (optional, for GUI commands)
- `cmake` ≥ 3.16 on PATH (for host tests)
- A C compiler on PATH for host tests (`gcc` or `clang`; `cl.exe` on Windows)

### Configuration

1. Copy `secrets.h.example` to `main/secrets.h` and fill in your values:
   ```c
   #define CONFIG_WIFI_SSID          "YourNetworkName"
   #define CONFIG_WIFI_PASSWORD      "YourPassword"
   #define CONFIG_OWM_API_KEY        "your-openweathermap-api-key"
   #define CONFIG_WEATHER_LOCATION   "48.8566,2.3522"   /* lat,lon */
   #define CONFIG_TZ_POSIX           "CET-1CEST,M3.5.0,M10.5.0/3"
   ```
2. Confirm `main/secrets.h` is listed in `.gitignore` before committing anything.

---

## Scenario 1 — Host Unit Tests (No Hardware Required)

**Validates**: SC-009 — weather parsing and time formatting on a host machine.

```bash
# From repo root
cd tests/host
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/host_tests
```

**Expected outcome**:
```
TEST(WeatherParser, ParsesCurrentTemp) PASS
TEST(WeatherParser, RejectsEmptyHourlyArray) PASS
TEST(WeatherParser, RetainsOldDataOnParseFailure) PASS
TEST(TimeFormat, FormatsHMS) PASS
TEST(TimeFormat, FormatsDate) PASS
TEST(AppConfig, ValidatesWifiSsid) PASS
TEST(AppConfig, RejectsHttpUrl) PASS
...
All tests pass.
```

All tests MUST pass before any firmware is flashed.

---

## Scenario 2 — Build and Flash Firmware

```bash
idf.py set-target esp32
idf.py menuconfig        # verify I2C pins, log level
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

**Expected serial output** (within 5 seconds of boot):
```
I (xxx) KRONOS: Booting Kronos v0.1.0
I (xxx) APP_CONFIG: Config loaded from NVS
I (xxx) WIFI_MANAGER: Connecting to 'YourNetworkName'...
I (xxx) WIFI_MANAGER: Connected. IP: 192.168.x.x
I (xxx) TIME_SYNC: SNTP sync started (server: pool.ntp.org)
I (xxx) TIME_SYNC: Time synchronized. Drift: -0.12 s
I (xxx) WEATHER_SERVICE: Fetching weather from api.openweathermap.org
I (xxx) WEATHER_SERVICE: Parse OK. Temp=21.5 Condition=801
I (xxx) DISPLAY: Render complete
```

**Expected display**:
- Line 1: `14:32:07` (HH:MM:SS, updating every second)
- Line 2: `Sun 03 Aug`
- Line 3: `21°C  Few clouds`
- Line 4-6: Hourly forecast slots
- Footer: `Synced`

---

## Scenario 3 — Reliable Local Time Display (US-1, SC-002)

1. Flash firmware (Scenario 2).
2. Observe display — time MUST update every second with no visible freeze.
3. Start a weather fetch by waiting for the refresh interval (or lowering it temporarily
   in NVS to 60 s via `idf.py -p ... monitor` + custom NVS write tool).
4. While weather fetch is in progress, confirm time continues updating without stall.

**Pass criteria**: Time digits change every second throughout; no freeze observed during
weather fetch window.

---

## Scenario 4 — Wi-Fi Outage Recovery (US-3, SC-004, SC-006)

1. Boot device with valid config — wait for "Synced" state.
2. Disable the Wi-Fi router (or move device out of range).
3. Observe display: status MUST change to `"Wi-Fi: offline"` within 30 s.
4. Confirm time continues updating (RTC-based).
5. Re-enable router.
6. Confirm device reconnects automatically (no reboot); status returns to `"Synced"`;
   weather data refreshes within 5 minutes.

**Pass criteria**:
- No crash/reboot loop during outage (SC-006).
- Automatic recovery within 5 minutes of network restore (SC-004).

---

## Scenario 5 — Weather Fetch Failure / Stale Data (US-4 Scenario 2, FR-015)

1. Set `CONFIG_OWM_API_KEY` to an invalid value in `secrets.h` and reflash.
2. Observe: device connects to Wi-Fi and syncs time.
3. Weather area MUST show `"Weather N/A"` (no prior data).
4. Confirm time and date continue updating normally.

For stale-data scenario:
1. Use a valid API key, wait for a successful fetch ("Synced" + weather visible).
2. Block internet access (router firewall rule for the device).
3. Wait for `2 × weather_refresh_interval_s` (default: 30 min).
4. Display MUST show stale indicator; time continues updating.

**Pass criteria**: `"Weather N/A"` shown on first failure; staleness indicator shown after
threshold; clock unaffected throughout.

---

## Scenario 6 — Seven Status States (US-5, SC-005)

Step through each state and confirm the footer indicator:

| State | How to trigger | Expected footer |
|---|---|---|
| Wi-Fi connecting | Boot with router off; turn on during boot | `"Wi-Fi: connecting"` |
| Time syncing | Boot with router on; watch startup | `"Time: syncing"` |
| Synced | Normal operation | `"Synced"` |
| Weather stale | Block internet for 2× interval | `"Weather stale"` |
| Weather N/A | Invalid API key | `"Weather N/A"` |
| Wi-Fi offline | Disable router post-sync | `"Wi-Fi: offline"` |
| Offline, no sync | Boot with router permanently off | `"Offline, no sync"` |

---

## Scenario 7 — On-Device Integration Tests

```bash
# Run ESP-IDF component tests on connected hardware
idf.py -T tests/integration/test_wifi_manager -p /dev/ttyUSB0
idf.py -T tests/integration/test_time_sync     -p /dev/ttyUSB0
idf.py -T tests/integration/test_weather_service -p /dev/ttyUSB0
```

**Expected**: All component tests report PASS in serial monitor output.

---

## Validation Checklist

- [ ] Scenario 1: All host unit tests pass
- [ ] Scenario 2: Firmware builds, flashes, and shows correct time on display
- [ ] Scenario 3: Time updates every second; no freeze during weather fetch
- [ ] Scenario 4: Wi-Fi outage handled; automatic recovery within 5 min
- [ ] Scenario 5: Invalid API key shows "Weather N/A"; stale threshold enforced
- [ ] Scenario 6: All seven status states visible with correct footer text
- [ ] Scenario 7: All on-device integration tests pass
- [ ] SC-008: `git grep -rn "wifi_password\|api_key" -- '*.c' '*.h'` returns no matches
       with actual credential values (only placeholder references in `secrets.h.example`)
