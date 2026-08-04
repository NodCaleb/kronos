/* host-testable: no ESP32 platform dependencies */
#include <stdio.h>
#include <time.h>

/* T024 — format struct tm as "HH:MM:SS"; returns chars written (snprintf semantics) */
#ifndef ESP_PLATFORM
/* guard confirms: no ESP-IDF headers below this line */
#endif
int time_format_hms(const struct tm *t, char *buf, size_t len)
{
    return snprintf(buf, len, "%02d:%02d:%02d",
                    t->tm_hour, t->tm_min, t->tm_sec);
}

/* T025 — format struct tm as "%a %d %b" (e.g. "Sun 03 Aug") */
#ifndef ESP_PLATFORM
/* guard confirms: no ESP-IDF headers below this line */
#endif
int time_format_date(const struct tm *t, char *buf, size_t len)
{
    char tmp[32];
    strftime(tmp, sizeof(tmp), "%a %d %b", t);
    return snprintf(buf, len, "%s", tmp);
}
