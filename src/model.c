#include "model.h"
#include "persist.h"
#include "app_config.h"
#include <string.h>

static WatchModel g_model;
static uint32_t g_last_rtc_refresh = 0;
static uint32_t g_last_low_bat_popup = 0;

static bool is_leap(uint16_t year)
{
    return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

static uint16_t days_before_month(uint16_t year, uint8_t month)
{
    static const uint16_t acc[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint16_t days = acc[month - 1U];
    if (month > 2U && is_leap(year)) {
        days++;
    }
    return days;
}

uint32_t model_datetime_to_epoch(const DateTime *dt)
{
    uint32_t days = 0;
    for (uint16_t y = 1970; y < dt->year; ++y) {
        days += is_leap(y) ? 366U : 365U;
    }
    days += days_before_month(dt->year, dt->month);
    days += (uint32_t)(dt->day - 1U);
    return days * 86400UL + (uint32_t)dt->hour * 3600UL + (uint32_t)dt->minute * 60UL + dt->second;
}

void model_epoch_to_datetime(uint32_t epoch, DateTime *out)
{
    uint32_t days = epoch / 86400UL;
    uint32_t sec = epoch % 86400UL;

    out->hour = (uint8_t)(sec / 3600UL);
    sec %= 3600UL;
    out->minute = (uint8_t)(sec / 60UL);
    out->second = (uint8_t)(sec % 60UL);

    uint16_t year = 1970;
    while (1) {
        uint16_t d = is_leap(year) ? 366U : 365U;
        if (days < d) {
            break;
        }
        days -= d;
        year++;
    }

    static const uint8_t dim_tbl[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t month = 1;
    while (month <= 12U) {
        uint8_t dim = dim_tbl[month - 1U];
        if (month == 2U && is_leap(year)) {
            dim = 29U;
        }
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }

    out->year = year;
    out->month = month;
    out->day = (uint8_t)(days + 1U);
    out->weekday = (uint8_t)(((epoch / 86400UL) + 4U) % 7U); /* 1970-01-01 Thursday */
}

static uint32_t rtc_counter_get(void)
{
    uint32_t high1, low, high2;
    do {
        high1 = RTC->CNTH;
        low = RTC->CNTL;
        high2 = RTC->CNTH;
    } while (high1 != high2);
    return (high1 << 16) | low;
}

static void rtc_wait_ready(void)
{
    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
    }
}

static void rtc_counter_set(uint32_t value)
{
    HAL_PWR_EnableBkUpAccess();
    rtc_wait_ready();
    RTC->CRL |= RTC_CRL_CNF;
    RTC->CNTH = (uint16_t)(value >> 16);
    RTC->CNTL = (uint16_t)(value & 0xFFFFU);
    RTC->CRL &= (uint16_t)~RTC_CRL_CNF;
    rtc_wait_ready();
}

void model_set_datetime(const DateTime *dt)
{
    rtc_counter_set(model_datetime_to_epoch(dt));
    model_epoch_to_datetime(rtc_counter_get(), &g_model.now);
}

static void refresh_time_from_rtc(void)
{
    model_epoch_to_datetime(rtc_counter_get(), &g_model.now);
}

static void default_datetime_if_invalid(void)
{
    uint32_t epoch = rtc_counter_get();
    if (epoch < 1735689600UL) {
        DateTime dt = {2026, 1, 1, 4, 12, 0, 0};
        model_set_datetime(&dt);
    }
}

void model_init(void)
{
    memset(&g_model, 0, sizeof(g_model));
    persist_init();
    bool first_boot = !persist_is_initialized();
    persist_load_settings(&g_model.settings);
    persist_load_alarm(&g_model.alarm);
    g_model.activity.goal = g_model.settings.goal;
    g_model.timer.remain_s = DEFAULT_TIMER_SECONDS;
#if APP_FEATURE_BATTERY
    g_model.battery_mv = 0;
    g_model.battery_percent = 100U;
    g_model.charging = false;
#else
    g_model.battery_mv = 5000U;
    g_model.battery_percent = 100U;
    g_model.charging = true;
#endif
    default_datetime_if_invalid();
    refresh_time_from_rtc();
    if (first_boot) {
        persist_save_settings(&g_model.settings);
        persist_save_alarm(&g_model.alarm);
    }
}

WatchModel *model_get(void)
{
    return &g_model;
}

void model_set_battery(uint16_t mv, uint8_t percent, bool charging)
{
    g_model.battery_mv = mv;
    g_model.battery_percent = percent;
    g_model.charging = charging;
}

void model_set_alarm_enabled(bool enabled)
{
    g_model.alarm.enabled = enabled;
    g_model.alarm.ringing = false;
    persist_save_alarm(&g_model.alarm);
}

void model_set_alarm_time(uint8_t hour, uint8_t minute)
{
    g_model.alarm.hour = hour % 24U;
    g_model.alarm.minute = minute % 60U;
    persist_save_alarm(&g_model.alarm);
}

void model_stopwatch_toggle(uint32_t now_ms)
{
    if (!g_model.stopwatch.running) {
        g_model.stopwatch.running = true;
        g_model.stopwatch.last_tick_ms = now_ms;
    } else {
        g_model.stopwatch.running = false;
    }
}

void model_stopwatch_reset(void)
{
    if (!g_model.stopwatch.running) {
        g_model.stopwatch.elapsed_ms = 0;
    }
}

void model_timer_toggle(uint32_t now_ms)
{
    if (!g_model.timer.running) {
        g_model.timer.running = true;
        g_model.timer.last_tick_ms = now_ms;
    } else {
        g_model.timer.running = false;
    }
}

void model_timer_reset(void)
{
    if (!g_model.timer.running) {
        g_model.timer.remain_s = DEFAULT_TIMER_SECONDS;
    }
}

void model_timer_adjust_seconds(int32_t delta_s)
{
    if (g_model.timer.running) {
        return;
    }
    int32_t value = (int32_t)g_model.timer.remain_s + delta_s;
    if (value < 10) {
        value = 10;
    }
    if (value > 35999) {
        value = 35999;
    }
    g_model.timer.remain_s = (uint32_t)value;
}

void model_set_watchface(uint8_t face)
{
    g_model.settings.watchface = face % 3U;
    persist_save_settings(&g_model.settings);
}

void model_cycle_watchface(int8_t dir)
{
    int8_t next = (int8_t)g_model.settings.watchface + dir;
    if (next < 0) {
        next = 2;
    }
    if (next > 2) {
        next = 0;
    }
    g_model.settings.watchface = (uint8_t)next;
    persist_save_settings(&g_model.settings);
}

void model_set_brightness(uint8_t level)
{
    g_model.settings.brightness = level > 3U ? 3U : level;
    persist_save_settings(&g_model.settings);
}

void model_set_vibrate(bool enabled)
{
    g_model.settings.vibrate = enabled;
    persist_save_settings(&g_model.settings);
}

void model_set_auto_wake(bool enabled)
{
    g_model.settings.auto_wake = enabled;
    persist_save_settings(&g_model.settings);
}

void model_set_dnd(bool enabled)
{
    g_model.settings.dnd = enabled;
    persist_save_settings(&g_model.settings);
}

void model_set_goal(uint32_t goal)
{
    if (goal < 1000U) {
        goal = 1000U;
    }
    if (goal > 30000U) {
        goal = 30000U;
    }
    g_model.activity.goal = goal;
    g_model.settings.goal = goal;
    persist_save_settings(&g_model.settings);
}

void model_feed_motion_steps(uint16_t steps)
{
    g_model.activity.steps += steps;
}

void model_note_user_activity(void)
{
#if ACTIVITY_DEMO_MODE
    g_model.activity.steps += 3U;
#endif
}

void model_ack_popup(void)
{
    g_model.popup = POPUP_NONE;
    g_model.popup_latched = false;
    g_model.alarm.ringing = false;
}

void model_snooze_alarm(void)
{
    uint32_t epoch = rtc_counter_get();
    g_model.alarm.ringing = false;
    g_model.alarm.snooze_until_epoch = epoch + 300U;
    g_model.popup = POPUP_NONE;
    g_model.popup_latched = false;
}

static void check_alarm(uint32_t epoch)
{
    if (g_model.alarm.ringing) {
        return;
    }

    if (!g_model.alarm.enabled) {
        return;
    }

    if (g_model.alarm.snooze_until_epoch != 0U) {
        if (epoch >= g_model.alarm.snooze_until_epoch) {
            g_model.alarm.snooze_until_epoch = 0U;
            g_model.alarm.ringing = true;
            g_model.popup = POPUP_ALARM;
            g_model.popup_latched = true;
        }
        return;
    }

    if (g_model.now.second == 0U && g_model.now.hour == g_model.alarm.hour && g_model.now.minute == g_model.alarm.minute) {
        g_model.alarm.ringing = true;
        g_model.popup = POPUP_ALARM;
        g_model.popup_latched = true;
    }
}

void model_tick(uint32_t now_ms)
{
    if (now_ms - g_last_rtc_refresh >= RTC_REFRESH_MS) {
        g_last_rtc_refresh = now_ms;
        refresh_time_from_rtc();
    }

    uint32_t epoch = rtc_counter_get();

    if (g_model.stopwatch.running) {
        uint32_t delta = now_ms - g_model.stopwatch.last_tick_ms;
        g_model.stopwatch.last_tick_ms = now_ms;
        g_model.stopwatch.elapsed_ms += delta;
    }

    if (g_model.timer.running) {
        if (now_ms - g_model.timer.last_tick_ms >= 1000U) {
            uint32_t sec = (now_ms - g_model.timer.last_tick_ms) / 1000U;
            g_model.timer.last_tick_ms += sec * 1000U;
            if (g_model.timer.remain_s > sec) {
                g_model.timer.remain_s -= sec;
            } else {
                g_model.timer.remain_s = 0;
                g_model.timer.running = false;
                g_model.popup = POPUP_TIMER_DONE;
                g_model.popup_latched = true;
            }
        }
    }

    check_alarm(epoch);

#if APP_FEATURE_BATTERY
    if (g_model.battery_percent <= LOW_BATTERY_THRESHOLD && !g_model.popup_latched) {
        if (now_ms - g_last_low_bat_popup > 600000U) {
            g_last_low_bat_popup = now_ms;
            g_model.popup = POPUP_LOW_BATTERY;
            g_model.popup_latched = true;
        }
    }
#endif

    if (g_model.popup == POPUP_NONE) {
        g_model.popup_latched = false;
    }
}
