#include "ui.h"
#include "app_config.h"
#include "display.h"
#include "key.h"
#include "model.h"
#include "power.h"
#include "vibe.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    PAGE_WATCHFACE = 0,
    PAGE_QUICK,
    PAGE_APPS,
    PAGE_ALARM,
    PAGE_STOPWATCH,
    PAGE_TIMER,
    PAGE_ACTIVITY,
    PAGE_SETTINGS,
    PAGE_TIMESET,
    PAGE_ABOUT,
    PAGE_COUNT
} PageId;

typedef struct {
    PageId current;
    PageId from;
    PageId to;
    bool animating;
    int8_t dir;
    uint32_t anim_start_ms;
    uint32_t last_input_ms;
    uint32_t last_render_ms;
    bool sleeping;
    bool dirty;
    uint8_t quick_index;
    uint8_t app_index;
    uint8_t settings_index;
    uint8_t alarm_field;
    uint8_t time_field;
    bool settings_editing;
    bool alarm_editing;
    DateTime edit_time;
    PopupType last_popup;
} UiState;

static UiState ui;

static const char * const app_names[] = {"Alarm", "Stopwatch", "Timer", "Activity", "Settings", "About"};
static const PageId app_pages[] = {PAGE_ALARM, PAGE_STOPWATCH, PAGE_TIMER, PAGE_ACTIVITY, PAGE_SETTINGS, PAGE_ABOUT};
#if APP_FEATURE_VIBRATION
static const char * const settings_items[] = {"Brightness", "Vibrate", "AutoWake", "DND", "Goal", "Face", "Time Set"};
#else
static const char * const settings_items[] = {"Brightness", "AutoWake", "DND", "Goal", "Face", "Time Set"};
#endif
static const char * const weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

#define QUICK_CARD_COUNT 4U
#if APP_FEATURE_VIBRATION
#define SETTINGS_INDEX_BRIGHTNESS 0U
#define SETTINGS_INDEX_VIBRATE    1U
#define SETTINGS_INDEX_AUTOWAKE   2U
#define SETTINGS_INDEX_DND        3U
#define SETTINGS_INDEX_GOAL       4U
#define SETTINGS_INDEX_FACE       5U
#define SETTINGS_INDEX_TIMESET    6U
#else
#define SETTINGS_INDEX_BRIGHTNESS 0U
#define SETTINGS_INDEX_AUTOWAKE   1U
#define SETTINGS_INDEX_DND        2U
#define SETTINGS_INDEX_GOAL       3U
#define SETTINGS_INDEX_FACE       4U
#define SETTINGS_INDEX_TIMESET    5U
#endif

static void ui_mark_dirty(void)
{
    ui.dirty = true;
}

void ui_request_render(void)
{
    ui_mark_dirty();
}

static void haptic_soft(void)
{
#if APP_FEATURE_VIBRATION
    WatchModel *m = model_get();
    if (m->settings.vibrate && !m->settings.dnd) {
        vibe_pulse(18);
    }
#endif
}

static void haptic_confirm(void)
{
#if APP_FEATURE_VIBRATION
    WatchModel *m = model_get();
    if (m->settings.vibrate && !m->settings.dnd) {
        vibe_pulse(35);
    }
#endif
}

static void ui_start_anim(PageId next, int8_t dir, uint32_t now_ms)
{
    if (next == ui.current) {
        return;
    }
    ui.from = ui.current;
    ui.to = next;
    ui.dir = dir;
    ui.anim_start_ms = now_ms;
    ui.animating = true;
    ui_mark_dirty();
}

static void ui_go(PageId next, int8_t dir, uint32_t now_ms)
{
    haptic_soft();
    ui_start_anim(next, dir, now_ms);
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t dim_tbl[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t dim = dim_tbl[(month - 1U) % 12U];
    bool leap = ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
    if (month == 2U && leap) {
        dim = 29U;
    }
    return dim;
}

static void clamp_edit_time(void)
{
    if (ui.edit_time.month < 1U) ui.edit_time.month = 1U;
    if (ui.edit_time.month > 12U) ui.edit_time.month = 12U;
    uint8_t dim = days_in_month(ui.edit_time.year, ui.edit_time.month);
    if (ui.edit_time.day < 1U) ui.edit_time.day = 1U;
    if (ui.edit_time.day > dim) ui.edit_time.day = dim;
    if (ui.edit_time.hour > 23U) ui.edit_time.hour = 23U;
    if (ui.edit_time.minute > 59U) ui.edit_time.minute = 59U;
}

static void draw_header(int16_t x, const char *title)
{
    display_draw_text_centered_5x7(x, 2, 128, title, true);
    display_draw_hline(x + 6, 11, 116, true);
}

static void draw_dots(int16_t ox, uint8_t count, uint8_t active, int16_t y, bool invert)
{
    bool c = !invert;
    int16_t total_w = (int16_t)(count * 6 - 2);
    int16_t x = ox + (OLED_WIDTH - total_w) / 2;
    for (uint8_t i = 0; i < count; ++i) {
        if (i == active) {
            display_fill_round_rect(x + i * 6, y, 4, 4, c);
        } else {
            display_draw_round_rect(x + i * 6, y, 4, 4, c);
        }
    }
}

static void draw_scrollbar(int16_t x, int16_t y, int16_t h, uint8_t total, uint8_t selected)
{
    if (total <= 1U) {
        return;
    }
    display_draw_round_rect(x, y, 4, h, true);
    int16_t thumb_h = APP_CLAMP(h / (int16_t)total, 8, h - 2);
    int16_t max_y = h - thumb_h - 2;
    int16_t thumb_y = y + 1 + (int16_t)(((uint32_t)selected * (uint32_t)max_y) / (uint32_t)(total - 1U));
    display_fill_round_rect(x + 1, thumb_y, 2, thumb_h, true);
}

static void draw_watchface(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    char line[24];

#if APP_FEATURE_BATTERY
    display_draw_battery_icon(ox + 6, 4, m->battery_percent, m->charging, false);
    snprintf(line, sizeof(line), "%02u/%02u %s", m->now.month, m->now.day, weekdays[m->now.weekday % 7U]);
    display_draw_text_5x7(ox + 30, 5, line, true);
#else
    snprintf(line, sizeof(line), "%02u/%02u %s", m->now.month, m->now.day, weekdays[m->now.weekday % 7U]);
    display_draw_text_5x7(ox + 8, 5, line, true);
    display_draw_text_5x7(ox + 102, 5, "USB", true);
#endif

    if (m->settings.watchface == 0U) {
        display_draw_big_digit(ox + 12, 18, m->now.hour / 10U, 3, true);
        display_draw_big_digit(ox + 31, 18, m->now.hour % 10U, 3, true);
        display_draw_big_colon(ox + 55, 21, 2, true, true);
        display_draw_big_digit(ox + 62, 18, m->now.minute / 10U, 3, true);
        display_draw_big_digit(ox + 81, 18, m->now.minute % 10U, 3, true);
        snprintf(line, sizeof(line), "S%02u  %5lu", m->now.second, (unsigned long)m->activity.steps);
        display_draw_text_centered_5x7(ox, 54, 128, line, true);
    } else if (m->settings.watchface == 1U) {
        display_draw_round_rect(ox + 8, 16, 112, 38, true);
        display_draw_text_centered_5x7(ox, 20, 128, "MINIMAL", true);
        snprintf(line, sizeof(line), "%02u:%02u", m->now.hour, m->now.minute);
        display_draw_text_centered_5x7(ox, 31, 128, line, true);
#if APP_FEATURE_BATTERY
        snprintf(line, sizeof(line), "%02us   %u%%", m->now.second, m->battery_percent);
#else
        snprintf(line, sizeof(line), "%02us   USB", m->now.second);
#endif
        display_draw_text_centered_5x7(ox, 43, 128, line, true);
    } else {
        display_draw_big_digit(ox + 10, 14, m->now.hour / 10U, 2, true);
        display_draw_big_digit(ox + 23, 14, m->now.hour % 10U, 2, true);
        display_draw_big_colon(ox + 39, 17, 1, true, (m->now.second % 2U) == 0U);
        display_draw_big_digit(ox + 45, 14, m->now.minute / 10U, 2, true);
        display_draw_big_digit(ox + 58, 14, m->now.minute % 10U, 2, true);
        display_fill_round_rect(ox + 74, 16, 44, 28, true);
        display_draw_text_5x7(ox + 83, 20, "GOAL", false);
        uint8_t p = (uint8_t)((m->activity.steps >= m->activity.goal) ? 100U : (m->activity.steps * 100U / m->activity.goal));
        display_draw_progress_bar(ox + 80, 29, 30, 8, p, true);
        display_draw_text_5x7(ox + 84, 39, p >= 100U ? "DONE" : "MOVE", false);
        snprintf(line, sizeof(line), "%04u", (unsigned)m->now.year);
        display_draw_text_5x7(ox + 8, 52, line, true);
    }

    draw_dots(ox, 3U, m->settings.watchface, 60, false);
}

static void draw_card_shell(int16_t ox, const char *title)
{
    display_fill_round_rect(ox + 6, 14, 116, 42, true);
    display_draw_text_centered_5x7(ox + 6, 18, 116, title, false);
}

static void draw_quick(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    char line[24];

    if (ui.quick_index == 0U) {
#if APP_FEATURE_BATTERY
        draw_card_shell(ox, "Battery");
        display_draw_battery_icon(ox + 18, 31, m->battery_percent, m->charging, true);
        snprintf(line, sizeof(line), "%u%%  %umV", m->battery_percent, m->battery_mv);
        display_draw_text_5x7(ox + 42, 32, line, false);
#else
        draw_card_shell(ox, "System");
        display_draw_info_icon(ox + 16, 27, true);
        display_draw_text_5x7(ox + 38, 28, "STLINK debug", false);
        display_draw_text_5x7(ox + 38, 38, "USB power", false);
#endif
    } else if (ui.quick_index == 1U) {
        draw_card_shell(ox, "Activity");
        display_draw_step_icon(ox + 18, 28, true);
        snprintf(line, sizeof(line), "%lu", (unsigned long)m->activity.steps);
        display_draw_text_5x7(ox + 44, 28, line, false);
        uint8_t p = (uint8_t)((m->activity.steps >= m->activity.goal) ? 100U : (m->activity.steps * 100U / m->activity.goal));
        display_draw_progress_bar(ox + 44, 39, 56, 8, p, true);
    } else if (ui.quick_index == 2U) {
        draw_card_shell(ox, "Alarm");
        display_draw_alarm_icon(ox + 16, 27, true);
        snprintf(line, sizeof(line), "%s %02u:%02u", m->alarm.enabled ? "ON" : "OFF", m->alarm.hour, m->alarm.minute);
        display_draw_text_5x7(ox + 38, 31, line, false);
    } else {
        draw_card_shell(ox, "Timer");
        display_draw_timer_icon(ox + 16, 27, true);
        snprintf(line, sizeof(line), "%02lu:%02lu", (unsigned long)(m->timer.remain_s / 60U), (unsigned long)(m->timer.remain_s % 60U));
        display_draw_text_5x7(ox + 42, 31, line, false);
    }

    draw_dots(ox, QUICK_CARD_COUNT, ui.quick_index, 58, false);
}

static void draw_apps(PageId page, int16_t ox)
{
    (void)page;
    uint8_t total = (uint8_t)(sizeof(app_names) / sizeof(app_names[0]));
    draw_header(ox, "Apps");
    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t idx = (ui.app_index / 4U) * 4U + i;
        if (idx >= total) {
            break;
        }
        int16_t y = 16 + i * 12;
        bool sel = idx == ui.app_index;
        if (sel) {
            display_fill_round_rect(ox + 6, y - 1, 112, 10, true);
        }
        display_draw_text_5x7(ox + 12, y + 1, app_names[idx], !sel);
        if (idx == 0U) display_draw_alarm_icon(ox + 100, y - 2, sel);
        else if (idx == 1U) display_draw_timer_icon(ox + 100, y - 2, sel);
        else if (idx == 2U) display_draw_timer_icon(ox + 100, y - 2, sel);
        else if (idx == 3U) display_draw_step_icon(ox + 100, y - 2, sel);
        else if (idx == 4U) display_draw_gear_icon(ox + 100, y - 2, sel);
        else display_draw_info_icon(ox + 100, y - 2, sel);
    }
    draw_scrollbar(ox + 121, 16, 40, total, ui.app_index);
}

static void draw_alarm(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    char line[24];
    draw_header(ox, "Alarm");
    display_draw_alarm_icon(ox + 8, 18, false);
    snprintf(line, sizeof(line), "%02u:%02u", m->alarm.hour, m->alarm.minute);
    display_draw_text_5x7(ox + 30, 18, line, true);

    for (uint8_t i = 0; i < 4; ++i) {
        int16_t y = 31 + i * 8;
        bool sel = i == ui.alarm_field;
        if (sel) {
            display_fill_round_rect(ox + 6, y - 1, 116, 8, true);
        }
        if (i == 0U) {
            snprintf(line, sizeof(line), "%c Enable  %s", (sel && ui.alarm_editing) ? '*' : ' ', m->alarm.enabled ? "ON" : "OFF");
        } else if (i == 1U) {
            snprintf(line, sizeof(line), "%c Hour    %02u", (sel && ui.alarm_editing) ? '*' : ' ', m->alarm.hour);
        } else if (i == 2U) {
            snprintf(line, sizeof(line), "%c Minute  %02u", (sel && ui.alarm_editing) ? '*' : ' ', m->alarm.minute);
        } else {
            snprintf(line, sizeof(line), "  Save and Back");
        }
        display_draw_text_5x7(ox + 10, y, line, !sel);
    }
}

static void draw_stopwatch(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    uint32_t total_cs = m->stopwatch.elapsed_ms / 10U;
    uint32_t cs = total_cs % 100U;
    uint32_t total_s = total_cs / 100U;
    uint32_t min = total_s / 60U;
    uint32_t sec = total_s % 60U;
    char line[24];
    draw_header(ox, "Stopwatch");
    snprintf(line, sizeof(line), "%02lu:%02lu.%02lu", (unsigned long)min, (unsigned long)sec, (unsigned long)cs);
    display_draw_text_centered_5x7(ox, 26, 128, line, true);
    display_draw_progress_bar(ox + 14, 40, 100, 10, (uint8_t)cs, false);
    display_draw_text_centered_5x7(ox, 56, 128, m->stopwatch.running ? "OK pause" : "OK start  DN reset", true);
}

static void draw_timer(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    char line[24];
    draw_header(ox, "Timer");
    snprintf(line, sizeof(line), "%02lu:%02lu", (unsigned long)(m->timer.remain_s / 60U), (unsigned long)(m->timer.remain_s % 60U));
    display_draw_text_centered_5x7(ox, 24, 128, line, true);
    uint8_t p = (uint8_t)((m->timer.remain_s * 100U) / 35999U);
    display_draw_progress_bar(ox + 14, 39, 100, 10, p, false);
    display_draw_text_centered_5x7(ox, 56, 128, m->timer.running ? "OK pause  DN stop" : "UP/DN 10s  HOLD 1m", true);
}

static void draw_activity(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    char line[24];
    draw_header(ox, "Activity");
    display_draw_step_icon(ox + 18, 22, false);
    snprintf(line, sizeof(line), "%lu steps", (unsigned long)m->activity.steps);
    display_draw_text_5x7(ox + 38, 23, line, true);
    uint8_t p = (uint8_t)((m->activity.steps >= m->activity.goal) ? 100U : (m->activity.steps * 100U / m->activity.goal));
    display_draw_progress_bar(ox + 18, 37, 92, 10, p, false);
    snprintf(line, sizeof(line), "Goal %lu", (unsigned long)m->activity.goal);
    display_draw_text_centered_5x7(ox, 51, 128, line, true);
    display_draw_text_centered_5x7(ox, 58, 128, ACTIVITY_DEMO_MODE ? "demo from key input" : "sensor step source", true);
}

static void draw_settings(PageId page, int16_t ox)
{
    (void)page;
    WatchModel *m = model_get();
    char line[32];
    uint8_t total = (uint8_t)(sizeof(settings_items) / sizeof(settings_items[0]));
    draw_header(ox, "Settings");
    for (uint8_t i = 0; i < 5; ++i) {
        uint8_t idx = (ui.settings_index / 5U) * 5U + i;
        if (idx >= total) {
            break;
        }
        int16_t y = 16 + i * 9;
        bool sel = idx == ui.settings_index;
        if (sel) {
            display_fill_round_rect(ox + 6, y - 1, 112, 8, true);
        }
#if APP_FEATURE_VIBRATION
        if (idx == SETTINGS_INDEX_BRIGHTNESS) snprintf(line, sizeof(line), "%c Bright   %u", (sel && ui.settings_editing) ? '*' : ' ', m->settings.brightness + 1U);
        else if (idx == SETTINGS_INDEX_VIBRATE) snprintf(line, sizeof(line), "  Vibrate  %s", m->settings.vibrate ? "ON" : "OFF");
        else if (idx == SETTINGS_INDEX_AUTOWAKE) snprintf(line, sizeof(line), "  AutoWake %s", m->settings.auto_wake ? "ON" : "OFF");
        else if (idx == SETTINGS_INDEX_DND) snprintf(line, sizeof(line), "  DND      %s", m->settings.dnd ? "ON" : "OFF");
        else if (idx == SETTINGS_INDEX_GOAL) snprintf(line, sizeof(line), "%c Goal     %lu", (sel && ui.settings_editing) ? '*' : ' ', (unsigned long)m->activity.goal);
        else if (idx == SETTINGS_INDEX_FACE) snprintf(line, sizeof(line), "%c Face     %u", (sel && ui.settings_editing) ? '*' : ' ', m->settings.watchface + 1U);
        else snprintf(line, sizeof(line), "  %s", settings_items[idx]);
#else
        if (idx == SETTINGS_INDEX_BRIGHTNESS) snprintf(line, sizeof(line), "%c Bright   %u", (sel && ui.settings_editing) ? '*' : ' ', m->settings.brightness + 1U);
        else if (idx == SETTINGS_INDEX_AUTOWAKE) snprintf(line, sizeof(line), "  AutoWake %s", m->settings.auto_wake ? "ON" : "OFF");
        else if (idx == SETTINGS_INDEX_DND) snprintf(line, sizeof(line), "  DND      %s", m->settings.dnd ? "ON" : "OFF");
        else if (idx == SETTINGS_INDEX_GOAL) snprintf(line, sizeof(line), "%c Goal     %lu", (sel && ui.settings_editing) ? '*' : ' ', (unsigned long)m->activity.goal);
        else if (idx == SETTINGS_INDEX_FACE) snprintf(line, sizeof(line), "%c Face     %u", (sel && ui.settings_editing) ? '*' : ' ', m->settings.watchface + 1U);
        else snprintf(line, sizeof(line), "  %s", settings_items[idx]);
#endif
        display_draw_text_5x7(ox + 10, y, line, !sel);
    }
    draw_scrollbar(ox + 121, 16, 41, total, ui.settings_index);
}

static void draw_timeset(PageId page, int16_t ox)
{
    (void)page;
    char line[24];
    draw_header(ox, "Time Set");
    snprintf(line, sizeof(line), "%04u-%02u-%02u", ui.edit_time.year, ui.edit_time.month, ui.edit_time.day);
    display_draw_text_centered_5x7(ox, 21, 128, line, true);
    snprintf(line, sizeof(line), "%02u:%02u", ui.edit_time.hour, ui.edit_time.minute);
    display_draw_text_centered_5x7(ox, 33, 128, line, true);
    const char *fields[] = {"Y", "M", "D", "H", "Min", "OK"};
    for (uint8_t i = 0; i < 6; ++i) {
        int16_t bx = ox + 5 + i * 20;
        bool sel = i == ui.time_field;
        if (sel) {
            display_fill_round_rect(bx, 48, 18, 10, true);
        }
        display_draw_text_centered_5x7(bx, 50, 18, fields[i], !sel);
    }
}

static void draw_about(PageId page, int16_t ox)
{
    (void)page;
    draw_header(ox, "About");
    display_draw_info_icon(ox + 10, 18, false);
    display_draw_text_5x7(ox + 28, 18, "F103 Watch v2", true);
    display_draw_text_5x7(ox + 10, 31, "0.96 I2C SSD1306", true);
    display_draw_text_5x7(ox + 10, 40, "Dirty-page refresh", true);
    display_draw_text_5x7(ox + 10, 49, "4-key refined UI", true);
    display_draw_text_centered_5x7(ox, 58, 128, "BACK return", true);
}

static void draw_popup(void)
{
    WatchModel *m = model_get();
    if (m->popup == POPUP_NONE) {
        return;
    }

    display_fill_round_rect(18, 18, 92, 28, true);
    if (m->popup == POPUP_ALARM) {
        display_draw_text_centered_5x7(18, 24, 92, "ALARM", false);
        display_draw_text_centered_5x7(18, 34, 92, "OK off BK snooze", false);
    } else if (m->popup == POPUP_TIMER_DONE) {
        display_draw_text_centered_5x7(18, 24, 92, "TIMER DONE", false);
        display_draw_text_centered_5x7(18, 34, 92, "OK dismiss", false);
    } else if (m->popup == POPUP_LOW_BATTERY) {
        display_draw_text_centered_5x7(18, 24, 92, "LOW BATTERY", false);
        display_draw_text_centered_5x7(18, 34, 92, "Please charge", false);
    }
}

static void draw_page(PageId page, int16_t ox)
{
    switch (page) {
        case PAGE_WATCHFACE: draw_watchface(page, ox); break;
        case PAGE_QUICK: draw_quick(page, ox); break;
        case PAGE_APPS: draw_apps(page, ox); break;
        case PAGE_ALARM: draw_alarm(page, ox); break;
        case PAGE_STOPWATCH: draw_stopwatch(page, ox); break;
        case PAGE_TIMER: draw_timer(page, ox); break;
        case PAGE_ACTIVITY: draw_activity(page, ox); break;
        case PAGE_SETTINGS: draw_settings(page, ox); break;
        case PAGE_TIMESET: draw_timeset(page, ox); break;
        case PAGE_ABOUT: draw_about(page, ox); break;
        default: break;
    }
}

static void handle_popup_event(const KeyEvent *e)
{
    WatchModel *m = model_get();
    if (m->popup == POPUP_NONE) {
        return;
    }
    if (e->type != KEY_EVENT_SHORT && e->type != KEY_EVENT_LONG) {
        return;
    }

    haptic_confirm();
    if (e->id == KEY_ID_OK) {
        model_ack_popup();
    } else if (e->id == KEY_ID_BACK) {
        if (m->popup == POPUP_ALARM) model_snooze_alarm();
        else model_ack_popup();
    }
    ui_mark_dirty();
}

static void change_setting(int8_t delta)
{
    WatchModel *m = model_get();
    switch (ui.settings_index) {
                case SETTINGS_INDEX_BRIGHTNESS: model_set_brightness((uint8_t)APP_CLAMP((int)m->settings.brightness + delta, 0, 3)); break;
        case SETTINGS_INDEX_GOAL: model_set_goal((uint32_t)APP_CLAMP((int)m->activity.goal + delta * 500, 1000, 30000)); break;
        case SETTINGS_INDEX_FACE: model_cycle_watchface(delta > 0 ? 1 : -1); break;
        default: break;
    }
    display_set_contrast(power_brightness_to_contrast(model_get()->settings.brightness));
    ui_mark_dirty();
}

static void adjust_alarm(int8_t delta)
{
    WatchModel *m = model_get();
    if (ui.alarm_field == 1U) {
        int v = (int)m->alarm.hour + delta;
        if (v < 0) v = 23;
        if (v > 23) v = 0;
        model_set_alarm_time((uint8_t)v, m->alarm.minute);
    } else if (ui.alarm_field == 2U) {
        int v = (int)m->alarm.minute + delta;
        if (v < 0) v = 59;
        if (v > 59) v = 0;
        model_set_alarm_time(m->alarm.hour, (uint8_t)v);
    }
    ui_mark_dirty();
}

static void adjust_time_field(int8_t delta)
{
    if (ui.time_field == 0U) {
        int y = (int)ui.edit_time.year + delta;
        ui.edit_time.year = (uint16_t)APP_CLAMP(y, 2024, 2099);
    } else if (ui.time_field == 1U) {
        int m = (int)ui.edit_time.month + delta;
        if (m < 1) m = 12;
        if (m > 12) m = 1;
        ui.edit_time.month = (uint8_t)m;
    } else if (ui.time_field == 2U) {
        int d = (int)ui.edit_time.day + delta;
        uint8_t dim = days_in_month(ui.edit_time.year, ui.edit_time.month);
        if (d < 1) d = dim;
        if (d > dim) d = 1;
        ui.edit_time.day = (uint8_t)d;
    } else if (ui.time_field == 3U) {
        int h = (int)ui.edit_time.hour + delta;
        if (h < 0) h = 23;
        if (h > 23) h = 0;
        ui.edit_time.hour = (uint8_t)h;
    } else if (ui.time_field == 4U) {
        int mm = (int)ui.edit_time.minute + delta;
        if (mm < 0) mm = 59;
        if (mm > 59) mm = 0;
        ui.edit_time.minute = (uint8_t)mm;
    }
    clamp_edit_time();
    ui_mark_dirty();
}

static void handle_page_event(const KeyEvent *e, uint32_t now_ms)
{
    WatchModel *m = model_get();

    if (ui.current == PAGE_WATCHFACE) {
        if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_UP) {
                model_cycle_watchface(-1);
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN) {
                model_cycle_watchface(1);
                haptic_soft();
            } else if (e->id == KEY_ID_OK) {
                ui_go(PAGE_APPS, 1, now_ms);
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_QUICK, -1, now_ms);
            }
            ui_mark_dirty();
        } else if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_BACK) {
            ui.sleeping = !ui.sleeping;
            display_sleep(ui.sleeping);
        }
    } else if (ui.current == PAGE_QUICK) {
        if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_UP) {
                ui.quick_index = (ui.quick_index + QUICK_CARD_COUNT - 1U) % QUICK_CARD_COUNT;
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN) {
                ui.quick_index = (ui.quick_index + 1U) % QUICK_CARD_COUNT;
                haptic_soft();
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_WATCHFACE, 1, now_ms);
            } else if (e->id == KEY_ID_OK) {
                PageId target = PAGE_ACTIVITY;
                if (ui.quick_index == 0U) target = PAGE_SETTINGS;
                else if (ui.quick_index == 1U) target = PAGE_ACTIVITY;
                else if (ui.quick_index == 2U) target = PAGE_ALARM;
                else target = PAGE_TIMER;
                ui_go(target, 1, now_ms);
            }
            ui_mark_dirty();
        }
    } else if (ui.current == PAGE_APPS) {
        uint8_t total = (uint8_t)(sizeof(app_names) / sizeof(app_names[0]));
        if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_UP && ui.app_index > 0U) {
                ui.app_index--;
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN && ui.app_index + 1U < total) {
                ui.app_index++;
                haptic_soft();
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_WATCHFACE, -1, now_ms);
            } else if (e->id == KEY_ID_OK) {
                ui_go(app_pages[ui.app_index], 1, now_ms);
            }
            ui_mark_dirty();
        }
    } else if (ui.current == PAGE_ALARM) {
        if (ui.alarm_editing) {
            if (e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT || e->type == KEY_EVENT_LONG) {
                if (e->id == KEY_ID_UP) adjust_alarm(1);
                else if (e->id == KEY_ID_DOWN) adjust_alarm(-1);
                else if (e->id == KEY_ID_OK || e->id == KEY_ID_BACK) {
                    ui.alarm_editing = false;
                    haptic_confirm();
                    ui_mark_dirty();
                }
            }
        } else if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_UP && ui.alarm_field > 0U) {
                ui.alarm_field--;
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN && ui.alarm_field < 3U) {
                ui.alarm_field++;
                haptic_soft();
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_APPS, -1, now_ms);
            } else if (e->id == KEY_ID_OK) {
                if (ui.alarm_field == 0U) {
                    model_set_alarm_enabled(!m->alarm.enabled);
                    haptic_confirm();
                } else if (ui.alarm_field == 1U || ui.alarm_field == 2U) {
                    ui.alarm_editing = true;
                    haptic_soft();
                } else {
                    ui_go(PAGE_APPS, -1, now_ms);
                }
                ui_mark_dirty();
            }
        }
    } else if (ui.current == PAGE_STOPWATCH) {
        if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_OK) {
                model_stopwatch_toggle(now_ms);
                haptic_confirm();
            } else if (e->id == KEY_ID_DOWN) {
                model_stopwatch_reset();
                haptic_soft();
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_APPS, -1, now_ms);
            }
            ui_mark_dirty();
        }
    } else if (ui.current == PAGE_TIMER) {
        if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_OK) {
                model_timer_toggle(now_ms);
                haptic_confirm();
            } else if (e->id == KEY_ID_UP) {
                model_timer_adjust_seconds(10);
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN) {
                if (m->timer.running) {
                    model_timer_toggle(now_ms);
                    haptic_confirm();
                } else {
                    model_timer_adjust_seconds(-10);
                    haptic_soft();
                }
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_APPS, -1, now_ms);
            }
            ui_mark_dirty();
        } else if ((e->type == KEY_EVENT_LONG || e->type == KEY_EVENT_REPEAT) && !m->timer.running) {
            if (e->id == KEY_ID_UP) {
                model_timer_adjust_seconds(60);
                ui_mark_dirty();
            } else if (e->id == KEY_ID_DOWN) {
                model_timer_adjust_seconds(-60);
                ui_mark_dirty();
            }
        }
    } else if (ui.current == PAGE_ACTIVITY) {
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
            ui_go(PAGE_APPS, -1, now_ms);
            ui_mark_dirty();
        }
    } else if (ui.current == PAGE_SETTINGS) {
        uint8_t total = (uint8_t)(sizeof(settings_items) / sizeof(settings_items[0]));
        if (ui.settings_editing) {
            if (e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT || e->type == KEY_EVENT_LONG) {
                if (e->id == KEY_ID_UP) {
                    change_setting(1);
                } else if (e->id == KEY_ID_DOWN) {
                    change_setting(-1);
                } else if (e->id == KEY_ID_OK || e->id == KEY_ID_BACK) {
                    ui.settings_editing = false;
                    haptic_confirm();
                    ui_mark_dirty();
                }
            }
        } else if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_UP && ui.settings_index > 0U) {
                ui.settings_index--;
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN && ui.settings_index + 1U < total) {
                ui.settings_index++;
                haptic_soft();
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_APPS, -1, now_ms);
            } else if (e->id == KEY_ID_OK) {
                        if (ui.settings_index == SETTINGS_INDEX_BRIGHTNESS || ui.settings_index == SETTINGS_INDEX_GOAL || ui.settings_index == SETTINGS_INDEX_FACE) {
                    ui.settings_editing = true;
                    haptic_soft();
#if APP_FEATURE_VIBRATION
                } else if (ui.settings_index == SETTINGS_INDEX_VIBRATE) {
                    model_set_vibrate(!m->settings.vibrate);
                    haptic_confirm();
#endif
                } else if (ui.settings_index == SETTINGS_INDEX_AUTOWAKE) {
                    model_set_auto_wake(!m->settings.auto_wake);
                    haptic_confirm();
                } else if (ui.settings_index == SETTINGS_INDEX_DND) {
                    model_set_dnd(!m->settings.dnd);
                    haptic_confirm();
                } else {
                    ui.edit_time = m->now;
                    ui.time_field = 0U;
                    clamp_edit_time();
                    ui_go(PAGE_TIMESET, 1, now_ms);
                }
                ui_mark_dirty();
            }
        }
    } else if (ui.current == PAGE_TIMESET) {
        if (e->type == KEY_EVENT_SHORT) {
            if (e->id == KEY_ID_UP) {
                adjust_time_field(1);
                haptic_soft();
            } else if (e->id == KEY_ID_DOWN) {
                adjust_time_field(-1);
                haptic_soft();
            } else if (e->id == KEY_ID_OK) {
                if (ui.time_field == 5U) {
                    model_set_datetime(&ui.edit_time);
                    haptic_confirm();
                    ui_go(PAGE_SETTINGS, -1, now_ms);
                } else {
                    ui.time_field++;
                    haptic_soft();
                    ui_mark_dirty();
                }
            } else if (e->id == KEY_ID_BACK) {
                ui_go(PAGE_SETTINGS, -1, now_ms);
            }
        }
    } else if (ui.current == PAGE_ABOUT) {
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
            ui_go(PAGE_APPS, -1, now_ms);
            ui_mark_dirty();
        }
    }
}

void ui_init(void)
{
    memset(&ui, 0, sizeof(ui));
    ui.current = PAGE_WATCHFACE;
    ui.last_input_ms = HAL_GetTick();
    ui.last_popup = POPUP_NONE;
    ui.dirty = true;
    display_set_contrast(power_brightness_to_contrast(model_get()->settings.brightness));
}

void ui_wake(void)
{
    ui.sleeping = false;
    display_sleep(false);
    ui.last_input_ms = HAL_GetTick();
    ui_mark_dirty();
}

bool ui_is_sleeping(void)
{
    return ui.sleeping;
}

void ui_tick(uint32_t now_ms)
{
    KeyEvent ev;
    while (key_pop_event(&ev)) {
        ui.last_input_ms = now_ms;
        if (ui.sleeping) {
            ui_wake();
            continue;
        }
        model_note_user_activity();
        if (model_get()->popup != POPUP_NONE) {
            handle_popup_event(&ev);
        } else {
            handle_page_event(&ev, now_ms);
        }
    }

    if (model_get()->settings.auto_wake && !ui.sleeping) {
        if (now_ms - ui.last_input_ms > SCREEN_SLEEP_MS) {
            ui.sleeping = true;
            display_sleep(true);
        }
    }

    if (ui.animating && now_ms - ui.anim_start_ms >= UI_ANIM_DURATION_MS) {
        ui.animating = false;
        ui.current = ui.to;
        ui_mark_dirty();
    }
}

bool ui_should_render(uint32_t now_ms)
{
    if (ui.sleeping) {
        return false;
    }

    if (model_get()->popup != ui.last_popup) {
        return true;
    }

    if (ui.dirty) {
        return true;
    }

    if (ui.animating) {
        return (now_ms - ui.last_render_ms) >= UI_FRAME_MS;
    }

    if (model_get()->popup != POPUP_NONE) {
        return (now_ms - ui.last_render_ms) >= UI_POPUP_REFRESH_MS;
    }

    switch (ui.current) {
        case PAGE_WATCHFACE:
        case PAGE_QUICK:
        case PAGE_ACTIVITY:
            return (now_ms - ui.last_render_ms) >= UI_CARD_REFRESH_MS;
        case PAGE_TIMER:
            return model_get()->timer.running && ((now_ms - ui.last_render_ms) >= UI_TIMER_REFRESH_MS);
        case PAGE_STOPWATCH:
            return model_get()->stopwatch.running && ((now_ms - ui.last_render_ms) >= UI_STOPWATCH_REFRESH_MS);
        default:
            return false;
    }
}

void ui_render(void)
{
    if (ui.sleeping) {
        return;
    }

    display_clear();

    if (ui.animating) {
        uint32_t elapsed = HAL_GetTick() - ui.anim_start_ms;
        if (elapsed > UI_ANIM_DURATION_MS) {
            elapsed = UI_ANIM_DURATION_MS;
        }
        int16_t shift = (int16_t)((elapsed * OLED_WIDTH) / UI_ANIM_DURATION_MS);
        int16_t from_x = (ui.dir > 0) ? -shift : shift;
        int16_t to_x = (ui.dir > 0) ? (OLED_WIDTH - shift) : (shift - OLED_WIDTH);
        draw_page(ui.from, from_x);
        draw_page(ui.to, to_x);
    } else {
        draw_page(ui.current, 0);
    }

    draw_popup();
    display_present();

    ui.last_render_ms = HAL_GetTick();
    ui.last_popup = model_get()->popup;
    ui.dirty = false;
}
