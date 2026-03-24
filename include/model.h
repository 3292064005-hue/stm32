#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    POPUP_NONE = 0,
    POPUP_ALARM,
    POPUP_TIMER_DONE,
    POPUP_LOW_BATTERY
} PopupType;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DateTime;

typedef struct {
    bool enabled;
    bool ringing;
    uint8_t hour;
    uint8_t minute;
    uint32_t snooze_until_epoch;
} AlarmState;

typedef struct {
    bool running;
    uint32_t elapsed_ms;
    uint32_t last_tick_ms;
} StopwatchState;

typedef struct {
    bool running;
    uint32_t remain_s;
    uint32_t last_tick_ms;
} TimerState;

typedef struct {
    uint32_t steps;
    uint32_t goal;
} ActivityState;

typedef struct {
    uint8_t brightness;
    bool vibrate;
    bool auto_wake;
    bool dnd;
    uint8_t watchface;
    uint32_t goal;
} SettingsState;

typedef struct {
    DateTime now;
    AlarmState alarm;
    StopwatchState stopwatch;
    TimerState timer;
    ActivityState activity;
    SettingsState settings;
    uint16_t battery_mv;
    uint8_t battery_percent;
    bool charging;
    PopupType popup;
    bool popup_latched;
} WatchModel;

void model_init(void);
void model_tick(uint32_t now_ms);
WatchModel *model_get(void);

void model_ack_popup(void);
void model_snooze_alarm(void);

void model_set_alarm_enabled(bool enabled);
void model_set_alarm_time(uint8_t hour, uint8_t minute);

void model_stopwatch_toggle(uint32_t now_ms);
void model_stopwatch_reset(void);

void model_timer_toggle(uint32_t now_ms);
void model_timer_reset(void);
void model_timer_adjust_seconds(int32_t delta_s);

void model_set_watchface(uint8_t face);
void model_cycle_watchface(int8_t dir);
void model_set_brightness(uint8_t level);
void model_set_vibrate(bool enabled);
void model_set_auto_wake(bool enabled);
void model_set_dnd(bool enabled);
void model_set_goal(uint32_t goal);

void model_feed_motion_steps(uint16_t steps);
void model_note_user_activity(void);

void model_set_battery(uint16_t mv, uint8_t percent, bool charging);

uint32_t model_datetime_to_epoch(const DateTime *dt);
void model_epoch_to_datetime(uint32_t epoch, DateTime *out);
void model_set_datetime(const DateTime *dt);

#endif
