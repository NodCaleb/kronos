#include "error_handler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void error_handler_fatal(const char *tag, const char *message, esp_err_t err)
{
    ESP_LOGE(tag, "%s (err=0x%x) — restarting in 1 s", message, (unsigned)err);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

void error_handler_warn(const char *tag, const char *message, esp_err_t err)
{
    ESP_LOGW(tag, "%s (err=0x%x)", message, (unsigned)err);
}
