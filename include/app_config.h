#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ---------------- Display ---------------- */
#define OLED_WIDTH                      128
#define OLED_HEIGHT                     64
#define OLED_BUFFER_SIZE                (OLED_WIDTH * OLED_HEIGHT / 8)

/* Reuse CubeMX generated hi2c1 */
#define OLED_I2C_HANDLE                 hi2c1
#define OLED_I2C_ADDRESS_7BIT           0x3C
#define OLED_I2C_ADDRESS                (OLED_I2C_ADDRESS_7BIT << 1)
#define OLED_I2C_TIMEOUT_MS             20
#define OLED_I2C_CHUNK_BYTES            32
#define OLED_RESET_ENABLED              0
#define OLED_RESET_GPIO_Port            GPIOB
#define OLED_RESET_Pin                  GPIO_PIN_0

/* ---------------- Feature switches ---------------- */
#define APP_FEATURE_BATTERY             0
#define APP_FEATURE_VIBRATION           0

/* ---------------- UI timing ---------------- */
#define UI_FPS                          15
#define UI_FRAME_MS                     (1000 / UI_FPS)
#define UI_ANIM_DURATION_MS             180
#define UI_POPUP_REFRESH_MS             180
#define UI_WATCHFACE_REFRESH_MS         1000
#define UI_CARD_REFRESH_MS              1000
#define UI_TIMER_REFRESH_MS             200
#define UI_STOPWATCH_REFRESH_MS         80
#define SCREEN_SLEEP_MS                 15000

#define BATTERY_SAMPLE_MS               30000
#define RTC_REFRESH_MS                  200
#define LOW_BATTERY_THRESHOLD           15

#define DEFAULT_STEP_GOAL               8000U
#define DEFAULT_TIMER_SECONDS           300U

#define ACTIVITY_DEMO_MODE              1

/* ---------------- Key pins ----------------
 * Logical order:
 * UP   -> PB11
 * DOWN -> PB1
 * OK   -> PA5
 * BACK -> PA3
 */
#define KEY_UP_GPIO_Port                GPIOB
#define KEY_UP_Pin                      GPIO_PIN_11
#define KEY_DOWN_GPIO_Port              GPIOB
#define KEY_DOWN_Pin                    GPIO_PIN_1
#define KEY_OK_GPIO_Port                GPIOA
#define KEY_OK_Pin                      GPIO_PIN_5
#define KEY_BACK_GPIO_Port              GPIOA
#define KEY_BACK_Pin                    GPIO_PIN_3

/* ---------------- Optional battery input ---------------- */
#define BATTERY_ADC_HANDLE              hadc1
#define BATTERY_ADC_CHANNEL             ADC_CHANNEL_0
#define BATTERY_DIVIDER_RATIO           2.0f
#define BATTERY_FULL_MV                 4200
#define BATTERY_EMPTY_MV                3300
#define CHARGE_DET_GPIO_Port            GPIOB
#define CHARGE_DET_Pin                  GPIO_PIN_14
#define CHARGE_DET_ENABLED              0

/* ---------------- RTC backup layout ---------------- */
#define BACKUP_MAGIC                    0x5A77U
#define BKP_DR_MAGIC                    RTC_BKP_DR1
#define BKP_DR_SETTINGS0                RTC_BKP_DR2
#define BKP_DR_SETTINGS1                RTC_BKP_DR3
#define BKP_DR_ALARM                    RTC_BKP_DR4

#define APP_CLAMP(v, minv, maxv)        ((v) < (minv) ? (minv) : ((v) > (maxv) ? (maxv) : (v)))

#endif
