#include "unity.h"
#include "time_sync.h"
#include <string.h>

static struct tm make_time(int hour, int min, int sec, int mday, int mon, int year, int wday)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_hour  = hour;
    t.tm_min   = min;
    t.tm_sec   = sec;
    t.tm_mday  = mday;
    t.tm_mon   = mon;   /* 0-based: January = 0 */
    t.tm_year  = year;  /* years since 1900 */
    t.tm_wday  = wday;  /* 0 = Sunday */
    return t;
}

/* time_format_hms: midnight → "00:00:00" */
static void test_hms_midnight(void)
{
    struct tm t = make_time(0, 0, 0, 1, 0, 125, 3);
    char buf[16];
    int ret = time_format_hms(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00:00:00", buf);
    TEST_ASSERT_EQUAL_INT(8, ret);
}

/* time_format_hms: noon → "12:00:00" */
static void test_hms_noon(void)
{
    struct tm t = make_time(12, 0, 0, 1, 0, 125, 3);
    char buf[16];
    int ret = time_format_hms(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("12:00:00", buf);
    TEST_ASSERT_EQUAL_INT(8, ret);
}

/* time_format_hms: 23:59:59 → "23:59:59" */
static void test_hms_end_of_day(void)
{
    struct tm t = make_time(23, 59, 59, 1, 0, 125, 3);
    char buf[16];
    int ret = time_format_hms(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("23:59:59", buf);
    TEST_ASSERT_EQUAL_INT(8, ret);
}

/* time_format_hms: buffer too small returns the full char count (snprintf semantics). */
static void test_hms_buffer_too_small(void)
{
    struct tm t = make_time(0, 0, 0, 1, 0, 125, 3);
    char buf[4];
    int ret = time_format_hms(&t, buf, sizeof(buf));
    /* "00:00:00" = 8 chars; snprintf returns 8 regardless of buffer size */
    TEST_ASSERT_EQUAL_INT(8, ret);
}

/* time_format_date: August 3, 2025 (Sunday) → "Sun 03 Aug" */
static void test_date_known_sunday(void)
{
    /* tm_mon=7 (August), tm_year=125 (2025), tm_wday=0 (Sunday) */
    struct tm t = make_time(0, 0, 0, 3, 7, 125, 0);
    char buf[16];
    time_format_date(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Sun 03 Aug", buf);
}

/* time_format_date: buffer too small returns the full char count. */
static void test_date_buffer_too_small(void)
{
    struct tm t = make_time(0, 0, 0, 3, 7, 125, 0);
    char buf[4];
    int ret = time_format_date(&t, buf, sizeof(buf));
    /* "Sun 03 Aug" = 10 chars; snprintf returns 10 */
    TEST_ASSERT_EQUAL_INT(10, ret);
}

void run_time_format_tests(void)
{
    RUN_TEST(test_hms_midnight);
    RUN_TEST(test_hms_noon);
    RUN_TEST(test_hms_end_of_day);
    RUN_TEST(test_hms_buffer_too_small);
    RUN_TEST(test_date_known_sunday);
    RUN_TEST(test_date_buffer_too_small);
}
