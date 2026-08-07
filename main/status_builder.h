#pragma once

#include <stddef.h>
#include "app_state.h"

/* Derive the display footer string from current wifi/time/weather state.
 * Priority order per data-model.md Status State Machine (SC-005). */
void build_status_string(const app_state_t *state, char *buf, size_t len);
