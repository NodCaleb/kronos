#include "display.h"
#include "ssd1306.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "display";

/* GPIO defaults matching sdkconfig.defaults */
#define DISPLAY_SDA_GPIO    21
#define DISPLAY_SCL_GPIO    22
#define DISPLAY_RESET_GPIO  (-1)    /* no hardware reset pin */
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64

/* Page (row) assignments for 64-px display (8 pages × 8 px) */
#define PAGE_TIME       0
#define PAGE_DATE       1
#define PAGE_WEATHER    2
#define PAGE_FORECAST_0 3
#define PAGE_FORECAST_1 4
#define PAGE_FORECAST_2 5
#define PAGE_STALE      6
#define PAGE_STATUS     7

static SSD1306_t s_dev;
static char      s_last_time[DISPLAY_TIME_LEN]; /* partial-redraw cache */

/* Write a 16-char wide row; left-justifies text and space-pads the remainder */
static void render_page(int page, const char *text)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%-16s", text ? text : "");
    ssd1306_display_text(&s_dev, page, buf, 16, false);
}

/* T019 — display_init: I2C + SSD1306 init, splash screen within 2 s */
esp_err_t display_init(void)
{
    i2c_master_init(&s_dev, DISPLAY_SDA_GPIO, DISPLAY_SCL_GPIO, DISPLAY_RESET_GPIO);
    ssd1306_init(&s_dev, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    ssd1306_clear_screen(&s_dev, false);
    ssd1306_contrast(&s_dev, 0xff);
    memset(s_last_time, 0, sizeof(s_last_time));
    display_show_message("Kronos v0.1.0", "Starting...");
    ESP_LOGI(TAG, "OLED initialised (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return ESP_OK;
}

/* T020 — display_render: render payload; partial redraw on time area
 * Zero calls to esp_sntp, esp_http_client, or any weather_* function (Principle V). */
void display_render(const display_payload_t *payload)
{
    if (!payload) {
        return;
    }

    /* Partial redraw: update time page only when the value changes */
    if (strncmp(payload->time_str, s_last_time, DISPLAY_TIME_LEN) != 0) {
        render_page(PAGE_TIME, payload->time_str);
        strncpy(s_last_time, payload->time_str, DISPLAY_TIME_LEN);
        s_last_time[DISPLAY_TIME_LEN - 1] = '\0';
    }

    render_page(PAGE_DATE, payload->date_str);
    render_page(PAGE_WEATHER, payload->weather_summary);

    for (int i = 0; i < DISPLAY_FORECAST_LINES; i++) {
        render_page(PAGE_FORECAST_0 + i, payload->forecast[i]);
    }

    render_page(PAGE_STALE,  payload->weather_stale ? "*Weather stale  " : "");
    render_page(PAGE_STATUS, payload->status);
}

/* T021 — display_show_message: two-line overlay (boot / error states) */
void display_show_message(const char *line1, const char *line2)
{
    ssd1306_clear_screen(&s_dev, false);
    render_page(2, line1);
    render_page(4, line2);
    memset(s_last_time, 0, sizeof(s_last_time)); /* force full redraw on next render */
}
