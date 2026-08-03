# Contract: Weather API — OpenWeatherMap One Call 3.0

**Component**: `weather_service` | **Date**: 2026-08-03

This contract documents the external HTTP interface consumed by `weather_service`.
All other modules are shielded from this contract; they interact only with
`weather_service.h` accessors.

---

## Endpoint

```
GET https://api.openweathermap.org/data/3.0/onecall
```

### Query Parameters

| Parameter | Required | Description | Example |
|---|---|---|---|
| `lat` | Yes | Latitude (decimal) | `48.8566` |
| `lon` | Yes | Longitude (decimal) | `2.3522` |
| `exclude` | Yes | Exclude unused parts | `minutely,daily,alerts` |
| `units` | Yes | `metric` or `imperial` | `metric` |
| `appid` | Yes | API key (from `DeviceConfig`) | `abc123...` |

### Request Headers

| Header | Value |
|---|---|
| `User-Agent` | `Kronos/1.0 ESP32` |

### TLS

- Certificate validation via ESP-IDF built-in CA bundle (`CONFIG_ESP_TLS_USING_MBEDTLS`).
- Plain HTTP (`http://`) MUST NOT be used under any circumstances (Constitution VII).
- `request_timeout_ms` from `DeviceConfig` applied to both connect and read phases.

---

## Response — HTTP 200 OK

### Relevant Fields

```json
{
  "current": {
    "dt": 1722700000,
    "temp": 21.5,
    "weather": [
      {
        "id": 801,
        "description": "few clouds"
      }
    ]
  },
  "hourly": [
    {
      "dt": 1722700000,
      "temp": 21.5,
      "weather": [
        {
          "id": 801,
          "description": "few clouds"
        }
      ]
    }
  ]
}
```

### Parser Extraction Rules

| Source path | Target field | Notes |
|---|---|---|
| `current.temp` | `WeatherData.current_temp` | Float; degrees in configured units |
| `current.weather[0].id` | `WeatherData.current_condition_code` | Integer; retained for icon rendering |
| `current.weather[0].description` | `WeatherData.current_condition_text` | English string; max 47 chars |
| `hourly[0..5].dt` | `hourly_slot_t.time_label` | Converted to `"HH:MM"` local time |
| `hourly[0..5].temp` | `hourly_slot_t.temperature` | Float |
| `hourly[0..5].weather[0].id` | `hourly_slot_t.condition_code` | Integer |
| `hourly[0..5].weather[0].description` | `hourly_slot_t.condition_text` | String; max 47 chars |

**Rejection rules** (parser MUST reject and retain old data):
- `current` object missing from response root.
- `current.temp` not a number.
- `current.weather` array empty or missing.
- `hourly` array missing (zero-slot forecast is not valid).

---

## Error Responses

| HTTP Status | Firmware Behaviour |
|---|---|
| `401 Unauthorized` | Log `ESP_LOGE` "API key invalid"; mark weather UNAVAILABLE; do not crash |
| `429 Too Many Requests` | Log `ESP_LOGW` "Rate limited"; retain stale data; back off before retry |
| `5xx Server Error` | Log `ESP_LOGW` "Server error {code}"; retain stale data; retry on next interval |
| Network timeout | Log `ESP_LOGW` "Request timed out"; retain stale data |
| TLS handshake failure | Log `ESP_LOGE` "TLS error"; retain stale data |
| Malformed JSON | Log `ESP_LOGE` "JSON parse error: {details}"; retain stale data (FR-014) |

---

## Rate Limiting

- Default refresh interval: 900 s (15 min) → ~96 calls/day.
- Free tier quota: 1,000 calls/day.
- On HTTP 429: next retry delayed by `min(current_interval × 2, 3600)` seconds.
