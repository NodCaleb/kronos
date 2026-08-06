# Deployment Guide: Kronos MVP

**Version**: 0.1.0 | **Target**: ESP32-WROOM-32 | **Framework**: ESP-IDF v5.x

---

## Prerequisites

- ESP-IDF v5.x installed and activated (`idf.py` on PATH)
- ESP32-WROOM-32 connected via USB
- Serial port identified (e.g., `/dev/ttyUSB0` on Linux, `COM3` on Windows)
- OpenWeatherMap account with a One Call 3.0 API key

---

## Step 1 — Configure Secrets

Copy the example secrets file and fill in your values:

```bash
cp main/secrets.h.example main/secrets.h
```

Edit `main/secrets.h`:

```c
#define CONFIG_WIFI_SSID        "YourNetworkName"
#define CONFIG_WIFI_PASSWORD    "YourWifiPassword"
#define CONFIG_OWM_API_KEY      "your-openweathermap-api-key"
#define CONFIG_WEATHER_LOCATION "lat=51.5074&lon=-0.1278"   /* latitude,longitude */
#define CONFIG_TZ_POSIX         "GMT0BST,M3.5.0/1,M10.5.0" /* POSIX TZ string */
```

Find your POSIX TZ string at [https://github.com/nayarsystems/posix_tz_db] or use
`timedatectl list-timezones` on Linux.

> **Security**: `main/secrets.h` is listed in `.gitignore` and must never be committed.
> Verify with `git status` before any commit.

---

## Step 2 — Set Target

```bash
idf.py set-target esp32
```

This configures the build for the ESP32-WROOM-32 (Xtensa LX6 dual-core).

---

## Step 3 — Review Build Configuration (Optional)

```bash
idf.py menuconfig
```

Key settings to review:

| Menu path | Setting | Default |
|---|---|---|
| Component config → ESP System Settings → Log output | Default log level | `INFO` |
| Component config → ESP-Driver-GPIO → I2C | SDA pin | 21 |
| Component config → ESP-Driver-GPIO → I2C | SCL pin | 22 |
| Component config → mbedTLS | Certificate bundle | Full (required for OWM HTTPS) |

`sdkconfig.defaults` pre-populates all required values. `menuconfig` is only needed if
you are changing I2C pins or log verbosity.

---

## Step 4 — Build

```bash
idf.py build
```

Verify with `idf.py size` that the binary fits within the `app0` partition (1792 KB).

---

## Step 5 — Flash

```bash
idf.py -p /dev/ttyUSB0 flash
```

Replace `/dev/ttyUSB0` with the correct serial port for your system (e.g., `COM3` on
Windows). The first flash writes:
- Firmware to `app0` partition
- `secrets.h` values to NVS namespace `kronos_cfg` on first boot

---

## Step 6 — Monitor

```bash
idf.py -p /dev/ttyUSB0 monitor
```

**Expected serial output within 5 seconds of boot**:

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
```
14:32:07
Sun 03 Aug
21°C  Few clouds
15:00  21°C  Few clouds
16:00  19°C  Clouds
17:00  18°C  Rain
Synced
```

Press `Ctrl+]` to exit the monitor.

---

## Updating Configuration (NVS)

After initial flash, configuration is stored in NVS and persists across reboots and OTA
updates. To change a value without reflashing the full firmware:

1. Edit `main/secrets.h` with the new values.
2. Rebuild and reflash: `idf.py build && idf.py -p /dev/ttyUSB0 flash`
3. On next boot `app_config_load()` will detect changed compile-time values and write
   them to NVS.

Alternatively, use the ESP-IDF NVS partition tool (`nvs_partition_gen.py`) to flash only
the NVS partition if you want to avoid reflashing the firmware image.

---

## OTA Update Path

The partition layout includes two app partitions (`app0` / `app1`) and an `otadata`
partition, enabling dual-OTA updates:

```
app0   0x10000   1792 KB   ota_0   ← active after first flash
app1   0x1D0000  1792 KB   ota_1   ← target for first OTA update
otadata 0x390000   8 KB   ota     ← tracks which partition is active
```

To perform an OTA update:
1. Build the new firmware image.
2. Host the `.bin` file on an HTTPS server accessible by the device.
3. Trigger the OTA update via the firmware's OTA update mechanism (see `esp_https_ota`
   documentation for implementation details — out of scope for MVP v0.1.0).

NVS data (configuration) is preserved across OTA updates because the NVS partition is
separate from both app partitions.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Display blank after boot | I2C wiring or pin config | Check SDA/SCL connections; verify menuconfig pins |
| `"Wi-Fi: connecting"` never clears | Wrong SSID/password | Re-check `secrets.h` and reflash |
| `"Weather N/A"` | Invalid or missing API key | Verify `CONFIG_OWM_API_KEY` in `secrets.h` |
| `"Time: syncing"` persists | NTP server unreachable | Confirm internet access; default server is `pool.ntp.org` |
| Build fails with missing header | `secrets.h` not created | Run `cp main/secrets.h.example main/secrets.h` |
| Flash fails | Wrong serial port | Check `idf.py -p <port>` argument; `dmesg` on Linux to find port |
