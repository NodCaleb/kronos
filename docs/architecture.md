# Architecture: Kronos MVP — Embedded IoT Digital Clock with Weather Display

**Version**: 0.1.0 | **Target**: ESP32-WROOM-32 | **Framework**: ESP-IDF v5.x

---

## Technology Stack

| Layer | Technology | Notes |
|---|---|---|
| Language | C17 | Primary; C++17 permitted for RAII |
| Framework | ESP-IDF v5.x | Latest stable, pinned in `idf_component.yml` |
| RTOS | FreeRTOS | Bundled with ESP-IDF |
| JSON | cJSON (`esp-cjson`) | Built-in ESP-IDF component |
| Display driver | `esp-idf-ssd1306` (nopnop2002) | SSD1306 / SH1106 over I2C |
| TLS | mbedTLS | Via `CONFIG_ESP_TLS_USING_MBEDTLS`; full CA bundle |
| Storage | ESP32 NVS | Namespace `kronos_cfg`; survives OTA updates |
| Test framework | Unity | Bundled with ESP-IDF; used for host and on-device tests |
| Config/secrets | `secrets.h` + NVS | `secrets.h` gitignored; NVS for runtime updates |

---

## Hardware Target

- **MCU**: ESP32-WROOM-32 — Xtensa LX6 dual-core 240 MHz
- **RAM**: 520 KB SRAM
- **Flash**: 4 MB
- **Display**: SSD1306 or SH1106 128×64 OLED over I2C (SDA=GPIO 21, SCL=GPIO 22)

---

## Seven Component Modules

| Module | Directory | One-line responsibility |
|---|---|---|
| `app_config` | `components/app_config/` | Load and validate device configuration from NVS and `secrets.h` |
| `app_state` | `components/app_state/` | Shared runtime state struct with FreeRTOS mutex protection |
| `error_handler` | `components/error_handler/` | Uniform logging and fatal/warn error dispatch |
| `wifi_manager` | `components/wifi_manager/` | Wi-Fi station init, connect, and exponential back-off reconnect |
| `time_sync` | `components/time_sync/` | SNTP init, POSIX TZ, periodic re-sync, and time formatting |
| `weather_service` | `components/weather_service/` | HTTPS weather fetch, cJSON parse, and stale-data management |
| `display` | `components/display/` | All OLED rendering; accepts only `display_payload_t` input |

`main/main.c` is the sole module-wiring point. All `_init()` and `_start()` calls live there.

---

## Inter-Module Dependency Graph

The dependency graph is **acyclic**. Arrows point from dependent → dependency.

```
main
 ├── app_config
 ├── app_state
 ├── error_handler
 ├── wifi_manager  ──► app_config, app_state
 ├── time_sync     ──► app_config, app_state, wifi_manager
 ├── weather_service ► app_config, app_state, wifi_manager
 └── display

wifi_manager, time_sync, weather_service all read wifi event group from wifi_manager.
display has NO dependency on any network or config module (Principle V).
```

Enforced by each component's `REQUIRES` list in its `CMakeLists.txt`. No `.c`-to-`.c`
includes exist across components.

---

## FreeRTOS Task Architecture

Four dedicated tasks share `app_state` through a mutex (`xSemaphoreCreateMutex`).

| Task | Priority | Stack | Spawned by | Responsibility |
|---|---|---|---|---|
| `clock_task` | 5 (highest) | 4 KB | `main.c` | Reads `app_state` every 1 s; builds `display_payload_t`; calls `display_render()` |
| `wifi_task` | 4 | 4 KB | `wifi_manager_start()` | Drives Wi-Fi connect/reconnect; exponential back-off; updates `app_state.wifi_state` |
| `ntp_task` | 3 | 3 KB | `time_sync_start()` | Waits on `WIFI_CONNECTED_BIT`; calls `esp_sntp_init()`; re-syncs every 3 h |
| `weather_task` | 3 | 6 KB | `weather_service_start()` | Waits on `WIFI_CONNECTED_BIT`; HTTPS fetch + parse on configurable interval |

`clock_task` uses `app_state_try_read()` with a 5 ms timeout to avoid priority inversion.
`wifi_task` and `ntp_task` / `weather_task` use `xEventGroupWaitBits()` on the
`EventGroupHandle_t` provided by `wifi_manager_get_event_group()`.

---

## Peak Memory Budget

| Region | Item | Estimate |
|---|---|---|
| Heap (peak, fetch cycle) | cJSON parse buffer + tree | ~100 KB |
| Heap (steady-state) | `app_state_t` + `device_config_t` structs | ~2 KB |
| Heap (steady-state) | mbedTLS TLS session | ~36 KB |
| Stack | 4 FreeRTOS tasks (avg 4 KB each) | ~17 KB |
| Flash (code) | Application + all components | ~600–800 KB estimated |

**Peak heap during weather fetch**: ~138 KB.
**Available SRAM** after FreeRTOS kernel (~50 KB) and Wi-Fi stack (~100 KB): ~370 KB.
Sufficient with margin. Verify with `esp_get_minimum_free_heap_size()` and `idf.py size`
after first full build.

---

## Dual-OTA Partition Layout

```
Offset     Size      Name       Type    SubType
0x9000     24 KB     nvs        data    nvs
0xf000      4 KB     phy_init   data    phy
0x10000  1792 KB     app0       app     ota_0
0x1D0000 1792 KB     app1       app     ota_1
0x390000    8 KB     otadata    data    ota
```

Both OTA partitions are present from day one (Principle XII). NVS is stored separately
and survives OTA updates — configuration persists across firmware upgrades.

---

## Initialisation Sequence

`app_main()` wires all modules in this order:

1. `app_config_load()` — load device config from NVS (writes `secrets.h` values on first boot)
2. `app_state_init()` — create shared state and mutex
3. `display_init()` — I2C + SSD1306 init; splash screen (`"Kronos v0.1.0"`) within 2 s
4. `wifi_manager_init()` — configure Wi-Fi stack in station mode
5. `wifi_manager_start()` — spawn `wifi_task`
6. `time_sync_init()` — configure SNTP server and POSIX TZ string
7. `time_sync_start()` — spawn `ntp_task`
8. `weather_service_init()` — store config and state pointers; construct request URL
9. `weather_service_start()` — spawn `weather_task`
10. `xTaskCreate(clock_task, ...)` — start display update loop

`error_handler` is called inline wherever a fatal or warning condition is detected; it is
not initialised separately.
