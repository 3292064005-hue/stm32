#include "persist.h"
#include "app_config.h"
#include "stm32f1xx_hal.h"

extern RTC_HandleTypeDef hrtc;

static uint16_t pack_settings0(const SettingsState *s)
{
    uint16_t v = 0;
    v |= (uint16_t)(s->brightness & 0x03U);
    v |= (uint16_t)((s->watchface & 0x03U) << 2);
    v |= (uint16_t)((s->vibrate ? 1U : 0U) << 4);
    v |= (uint16_t)((s->auto_wake ? 1U : 0U) << 5);
    v |= (uint16_t)((s->dnd ? 1U : 0U) << 6);
    return v;
}

static void unpack_settings0(uint16_t v, SettingsState *s)
{
    s->brightness = (uint8_t)(v & 0x03U);
    s->watchface = (uint8_t)((v >> 2) & 0x03U);
    s->vibrate = ((v >> 4) & 0x01U) != 0U;
    s->auto_wake = ((v >> 5) & 0x01U) != 0U;
    s->dnd = ((v >> 6) & 0x01U) != 0U;
}

void persist_init(void)
{
    HAL_PWR_EnableBkUpAccess();
}

bool persist_is_initialized(void)
{
    return HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_MAGIC) == BACKUP_MAGIC;
}

void persist_save_settings(const SettingsState *settings)
{
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_MAGIC, BACKUP_MAGIC);
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_SETTINGS0, pack_settings0(settings));
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_SETTINGS1, (uint16_t)(settings->goal > 65535U ? 65535U : settings->goal));
}

void persist_load_settings(SettingsState *settings)
{
    if (!persist_is_initialized()) {
        settings->brightness = 2;
        settings->watchface = 0;
        settings->vibrate = true;
        settings->auto_wake = true;
        settings->dnd = false;
        settings->goal = DEFAULT_STEP_GOAL;
        return;
    }
    unpack_settings0(HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_SETTINGS0), settings);
    settings->goal = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_SETTINGS1);
    if (settings->goal < 1000U) {
        settings->goal = DEFAULT_STEP_GOAL;
    }
}

void persist_save_alarm(const AlarmState *alarm)
{
    uint16_t v = 0;
    v |= (uint16_t)(alarm->hour & 0x1FU);
    v |= (uint16_t)((alarm->minute & 0x3FU) << 5);
    v |= (uint16_t)((alarm->enabled ? 1U : 0U) << 11);
    HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_ALARM, v);
}

void persist_load_alarm(AlarmState *alarm)
{
    if (!persist_is_initialized()) {
        alarm->enabled = false;
        alarm->hour = 7;
        alarm->minute = 30;
        alarm->ringing = false;
        alarm->snooze_until_epoch = 0;
        return;
    }

    uint16_t v = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_ALARM);
    alarm->hour = (uint8_t)(v & 0x1FU);
    alarm->minute = (uint8_t)((v >> 5) & 0x3FU);
    alarm->enabled = ((v >> 11) & 0x01U) != 0U;
    alarm->ringing = false;
    alarm->snooze_until_epoch = 0;
}
