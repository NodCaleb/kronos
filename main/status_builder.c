#include <string.h>
#include "status_builder.h"

void build_status_string(const app_state_t *state, char *buf, size_t len)
{
    switch (state->wifi_state) {
        case WIFI_STATE_INIT:
        case WIFI_STATE_CONNECTING:
            strncpy(buf, "Wi-Fi: connecting", len - 1);
            break;
        case WIFI_STATE_OFFLINE:
            strncpy(buf, (state->time_state == TIME_STATE_SYNCED)
                         ? "Wi-Fi: offline" : "Offline, no sync", len - 1);
            break;
        case WIFI_STATE_CONNECTED:
        default:
            if (state->time_state != TIME_STATE_SYNCED) {
                strncpy(buf, "Time: syncing", len - 1);
            } else if (state->weather.freshness == WEATHER_STALE) {
                strncpy(buf, "Weather stale", len - 1);
            } else if (state->weather.freshness == WEATHER_UNAVAILABLE) {
                strncpy(buf, "Weather N/A", len - 1);
            } else {
                strncpy(buf, "Synced", len - 1);
            }
            break;
    }
    buf[len - 1] = '\0';
}
