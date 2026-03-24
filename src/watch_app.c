#include "watch_app.h"
#include "app_config.h"
#include "display.h"
#include "key.h"
#include "model.h"
#include "power.h"
#include "ui.h"
#include "vibe.h"
#include "stm32f1xx_hal.h"

#if APP_FEATURE_BATTERY
#include <stdbool.h>
extern ADC_HandleTypeDef hadc1;
static uint16_t watch_app_adc_to_mv(uint32_t adc)
{
    float vref = 3300.0f;
    float mv = ((float)adc * vref / 4095.0f) * BATTERY_DIVIDER_RATIO;
    if (mv < 0.0f) {
        mv = 0.0f;
    }
    return (uint16_t)(mv + 0.5f);
}

static uint8_t watch_app_mv_to_percent(uint16_t mv)
{
    static const uint16_t mv_table[] = {3300, 3500, 3650, 3750, 3850, 3920, 3980, 4040, 4100, 4160, 4200};
    static const uint8_t pct_table[] = {0, 5, 12, 25, 45, 60, 72, 84, 92, 97, 100};

    if (mv <= mv_table[0]) {
        return 0;
    }
    if (mv >= mv_table[10]) {
        return 100;
    }

    for (uint8_t i = 1; i < 11; ++i) {
        if (mv < mv_table[i]) {
            uint16_t mv_lo = mv_table[i - 1U];
            uint16_t mv_hi = mv_table[i];
            uint8_t pct_lo = pct_table[i - 1U];
            uint8_t pct_hi = pct_table[i];
            uint32_t span_mv = (uint32_t)(mv_hi - mv_lo);
            uint32_t span_pct = (uint32_t)(pct_hi - pct_lo);
            return (uint8_t)(pct_lo + (((uint32_t)(mv - mv_lo) * span_pct) / span_mv));
        }
    }
    return 100;
}

static void watch_app_sample_battery(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 8U; ++i) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    uint16_t mv = watch_app_adc_to_mv(sum / 8U);
    uint8_t percent = watch_app_mv_to_percent(mv);
#if CHARGE_DET_ENABLED
    bool charging = HAL_GPIO_ReadPin(CHARGE_DET_GPIO_Port, CHARGE_DET_Pin) == GPIO_PIN_RESET;
#else
    bool charging = false;
#endif
    model_set_battery(mv, percent, charging);
}
#endif

static uint32_t g_last_key_scan = 0;
static uint32_t g_last_battery = 0;

void watch_app_init(void)
{
    key_init();
    power_init();
    display_init();
    vibe_init();
    model_init();
#if APP_FEATURE_BATTERY
    watch_app_sample_battery();
#else
    model_set_battery(5000U, 100U, true);
#endif
    ui_init();
    ui_request_render();
}

void watch_app_task(void)
{
    uint32_t now = HAL_GetTick();

    if (now - g_last_key_scan >= 10U) {
        g_last_key_scan = now;
        key_scan_10ms();
    }

    model_tick(now);
    ui_tick(now);

#if APP_FEATURE_BATTERY
    if (now - g_last_battery >= BATTERY_SAMPLE_MS) {
        g_last_battery = now;
        watch_app_sample_battery();
        ui_request_render();
    }
#endif

    vibe_tick(now, model_get()->popup, model_get()->settings.vibrate && !model_get()->settings.dnd);

    if (ui_should_render(now)) {
        ui_render();
    }
}

void watch_app_request_render(void)
{
    ui_request_render();
}
