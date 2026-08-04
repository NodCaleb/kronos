#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* ── Display geometry constants ──────────────────────────────────────────── */
#define DISPLAY_TIME_LEN        9   /* "HH:MM:SS\0" */
#define DISPLAY_DATE_LEN        12  /* "Mon DD Mon\0" + pad */
#define DISPLAY_WEATHER_LEN     24
#define DISPLAY_FORECAST_LINES  3
#define DISPLAY_FORECAST_LEN    24
#define DISPLAY_STATUS_LEN      20

/* ── Payload passed from clock_task to display_render() ─────────────────── */
typedef struct {
    char time_str[DISPLAY_TIME_LEN];                         /* "14:32:07" */
    char date_str[DISPLAY_DATE_LEN];                         /* "Sun 03 Aug" */
    char weather_summary[DISPLAY_WEATHER_LEN];               /* "21°C  Few clouds" */
    char forecast[DISPLAY_FORECAST_LINES][DISPLAY_FORECAST_LEN]; /* "15:00  19°C  Rain" */
    char status[DISPLAY_STATUS_LEN];                         /* "Wi-Fi: offline" */
    bool weather_stale;
} display_payload_t;

/* ── Public API ──────────────────────────────────────────────────────────── */
esp_err_t display_init(void);
void      display_render(const display_payload_t *payload);
void      display_show_message(const char *line1, const char *line2);
