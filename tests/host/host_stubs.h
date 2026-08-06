#ifndef HOST_STUBS_H
#define HOST_STUBS_H

/* Minimal ESP-IDF stubs for host (non-ESP32) compilation.
 * Setting KRONOS_HOST_ESP_ERR and KRONOS_HOST_FREERTOS suppresses
 * re-definition inside app_config.h and app_state.h. */

#define KRONOS_HOST_ESP_ERR
#define KRONOS_HOST_FREERTOS

typedef int esp_err_t;
#define ESP_OK                    0
#define ESP_ERR_INVALID_ARG      (-1)
#define ESP_ERR_INVALID_RESPONSE (-2)

typedef unsigned int TickType_t;
#define portMAX_DELAY  ((TickType_t)0xffffffffU)

#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) ((void)0)

/* localtime_r is POSIX; on Windows MinGW use localtime_s with swapped args */
#ifdef _WIN32
#  include <time.h>
static inline struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    localtime_s(result, timep);
    return result;
}
#endif

#endif /* HOST_STUBS_H */
