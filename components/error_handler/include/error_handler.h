#pragma once

/* ── Host/target portability ─────────────────────────────────────────────── */
#ifndef ESP_PLATFORM
#  ifndef KRONOS_HOST_ESP_ERR
#  define KRONOS_HOST_ESP_ERR
   typedef int esp_err_t;
#  define ESP_OK              0
#  define ESP_ERR_INVALID_ARG (-1)
#  endif
#else
#  include "esp_err.h"
#endif

/* ── Public API ──────────────────────────────────────────────────────────── */

void error_handler_fatal(const char *tag, const char *message, esp_err_t err);
void error_handler_warn(const char *tag, const char *message, esp_err_t err);
