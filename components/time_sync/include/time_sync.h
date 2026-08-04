#pragma once

#include <stdbool.h>
#include <time.h>
#include "app_state.h"  /* canonical owner of time_sync_state_t */
#include "app_config.h" /* device_config_t */

/* ── Lifecycle ───────────────────────────────────────────────────────────── */
esp_err_t time_sync_init(const device_config_t *config);
void      time_sync_start(void);

/* ── Accessors (safe from any task) ─────────────────────────────────────── */
time_sync_state_t time_sync_get_state(void);
bool              time_sync_get_local_time(struct tm *out_tm);

/* ── Formatting (host-testable — no ESP32 platform deps) ─────────────────── */
int time_format_hms(const struct tm *t, char *buf, size_t len);
int time_format_date(const struct tm *t, char *buf, size_t len);
