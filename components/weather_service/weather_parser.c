/* weather_parser.c — OWM One Call 3.0 JSON parser (host-testable)
 * No ESP32 platform dependencies; guarded with #ifdef HOST_BUILD for host tests.
 */

#ifdef HOST_BUILD
#include "../../tests/host/host_stubs.h"
#include <stdio.h>
#include <time.h>
#else
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <time.h>
#endif

#include <string.h>
#include <stddef.h>
#include "cJSON.h"
#include "weather_service.h"

static const char *TAG = "weather_parser";

/* Convert a Unix timestamp to "HH:MM" local time string. */
static void timestamp_to_hhmm(int64_t ts, char *buf, size_t len)
{
    time_t t = (time_t)ts;
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    snprintf(buf, len, "%02d:%02d", tm_local.tm_hour, tm_local.tm_min);
}

/* Safely copy a cJSON string value into a fixed buffer; returns false on failure. */
static bool copy_string(const cJSON *item, char *dst, size_t dst_len)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    strncpy(dst, item->valuestring, dst_len - 1);
    dst[dst_len - 1] = '\0';
    return true;
}

esp_err_t weather_parse(const char *json, size_t len, weather_data_t *out)
{
    if (json == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Parse into a temporary struct; only write to *out on full success. */
    weather_data_t tmp;
    memset(&tmp, 0, sizeof(tmp));

    cJSON *root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        const char *err = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "JSON parse error: %s", err ? err : "unknown");
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* ── current ─────────────────────────────────────────────────────── */
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    if (!cJSON_IsObject(current)) {
        ESP_LOGE(TAG, "JSON parse error: 'current' object missing");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *temp = cJSON_GetObjectItemCaseSensitive(current, "temp");
    if (!cJSON_IsNumber(temp)) {
        ESP_LOGE(TAG, "JSON parse error: 'current.temp' is not a number");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    tmp.current_temp = (float)temp->valuedouble;

    const cJSON *weather_arr = cJSON_GetObjectItemCaseSensitive(current, "weather");
    if (!cJSON_IsArray(weather_arr) || cJSON_GetArraySize(weather_arr) == 0) {
        ESP_LOGE(TAG, "JSON parse error: 'current.weather' array empty or missing");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *cond0 = cJSON_GetArrayItem(weather_arr, 0);
    const cJSON *cond_id   = cJSON_GetObjectItemCaseSensitive(cond0, "id");
    const cJSON *cond_desc = cJSON_GetObjectItemCaseSensitive(cond0, "description");

    if (!cJSON_IsNumber(cond_id) || (int)cond_id->valuedouble == 0) {
        ESP_LOGE(TAG, "JSON parse error: 'current.weather[0].id' invalid or zero");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    tmp.current_condition_code = (int)cond_id->valuedouble;

    if (!copy_string(cond_desc, tmp.current_condition_text, sizeof(tmp.current_condition_text))) {
        ESP_LOGE(TAG, "JSON parse error: 'current.weather[0].description' missing or invalid");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* ── hourly ──────────────────────────────────────────────────────── */
    const cJSON *hourly_arr = cJSON_GetObjectItemCaseSensitive(root, "hourly");
    if (!cJSON_IsArray(hourly_arr)) {
        ESP_LOGE(TAG, "JSON parse error: 'hourly' array missing");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    int total_slots = cJSON_GetArraySize(hourly_arr);
    if (total_slots == 0) {
        ESP_LOGE(TAG, "JSON parse error: 'hourly' array is empty (zero-slot forecast invalid)");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    int parse_slots = total_slots < WEATHER_FORECAST_SLOTS ? total_slots : WEATHER_FORECAST_SLOTS;
    tmp.forecast_count = 0;

    for (int i = 0; i < parse_slots; i++) {
        const cJSON *slot     = cJSON_GetArrayItem(hourly_arr, i);
        const cJSON *slot_dt  = cJSON_GetObjectItemCaseSensitive(slot, "dt");
        const cJSON *slot_tmp = cJSON_GetObjectItemCaseSensitive(slot, "temp");
        const cJSON *slot_w   = cJSON_GetObjectItemCaseSensitive(slot, "weather");

        if (!cJSON_IsNumber(slot_dt) || !cJSON_IsNumber(slot_tmp)) {
            continue; /* skip malformed slots silently */
        }
        if (!cJSON_IsArray(slot_w) || cJSON_GetArraySize(slot_w) == 0) {
            continue;
        }

        const cJSON *sw0      = cJSON_GetArrayItem(slot_w, 0);
        const cJSON *sw_id    = cJSON_GetObjectItemCaseSensitive(sw0, "id");
        const cJSON *sw_desc  = cJSON_GetObjectItemCaseSensitive(sw0, "description");

        if (!cJSON_IsNumber(sw_id)) {
            continue;
        }

        hourly_slot_t *s = &tmp.forecast[tmp.forecast_count];
        timestamp_to_hhmm((int64_t)slot_dt->valuedouble, s->time_label, sizeof(s->time_label));
        s->temperature    = (float)slot_tmp->valuedouble;
        s->condition_code = (int)sw_id->valuedouble;
        if (!copy_string(sw_desc, s->condition_text, sizeof(s->condition_text))) {
            s->condition_text[0] = '\0';
        }
        tmp.forecast_count++;
    }

    /* ── Commit ──────────────────────────────────────────────────────── */
    tmp.fetch_timestamp_s = (int64_t)time(NULL);
    tmp.freshness         = WEATHER_FRESH;

    cJSON_Delete(root);
    *out = tmp;
    return ESP_OK;
}
