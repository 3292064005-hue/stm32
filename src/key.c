#include "key.h"
#include "app_config.h"
#include "stm32f1xx_hal.h"

#define KEY_QUEUE_SIZE      16
#define KEY_DEBOUNCE_TICKS  3
#define KEY_LONG_TICKS      50
#define KEY_REPEAT_START    70
#define KEY_REPEAT_RATE     12

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    bool active_low;
    bool stable_down;
    bool last_sample;
    uint8_t debounce;
    uint16_t hold_ticks;
    bool long_sent;
} KeyState;

static KeyState g_keys[KEY_ID_COUNT];
static KeyEvent g_queue[KEY_QUEUE_SIZE];
static uint8_t g_head = 0;
static uint8_t g_tail = 0;

static void key_push_event(KeyId id, KeyEventType type)
{
    uint8_t next = (uint8_t)((g_head + 1U) % KEY_QUEUE_SIZE);
    if (next == g_tail) {
        return;
    }
    g_queue[g_head].id = id;
    g_queue[g_head].type = type;
    g_head = next;
}

static bool read_physical(KeyState *key)
{
    GPIO_PinState state = HAL_GPIO_ReadPin(key->port, key->pin);
    bool raw = (state == GPIO_PIN_SET);
    return key->active_low ? !raw : raw;
}

void key_init(void)
{
    g_keys[KEY_ID_UP]   = (KeyState){KEY_UP_GPIO_Port, KEY_UP_Pin, true, false, false, 0, 0, false};
    g_keys[KEY_ID_DOWN] = (KeyState){KEY_DOWN_GPIO_Port, KEY_DOWN_Pin, true, false, false, 0, 0, false};
    g_keys[KEY_ID_OK]   = (KeyState){KEY_OK_GPIO_Port, KEY_OK_Pin, true, false, false, 0, 0, false};
    g_keys[KEY_ID_BACK] = (KeyState){KEY_BACK_GPIO_Port, KEY_BACK_Pin, true, false, false, 0, 0, false};
}

void key_scan_10ms(void)
{
    for (uint8_t i = 0; i < KEY_ID_COUNT; ++i) {
        KeyState *k = &g_keys[i];
        bool sample = read_physical(k);

        if (sample == k->last_sample) {
            if (k->debounce < KEY_DEBOUNCE_TICKS) {
                k->debounce++;
            }
        } else {
            k->debounce = 0;
            k->last_sample = sample;
        }

        if (k->debounce >= KEY_DEBOUNCE_TICKS && sample != k->stable_down) {
            k->stable_down = sample;
            if (sample) {
                k->hold_ticks = 0;
                k->long_sent = false;
                key_push_event((KeyId)i, KEY_EVENT_PRESS);
            } else {
                if (!k->long_sent) {
                    key_push_event((KeyId)i, KEY_EVENT_SHORT);
                }
                key_push_event((KeyId)i, KEY_EVENT_RELEASE);
            }
        }

        if (k->stable_down) {
            k->hold_ticks++;
            if (!k->long_sent && k->hold_ticks >= KEY_LONG_TICKS) {
                k->long_sent = true;
                key_push_event((KeyId)i, KEY_EVENT_LONG);
            } else if (k->long_sent && k->hold_ticks >= KEY_REPEAT_START) {
                if (((k->hold_ticks - KEY_REPEAT_START) % KEY_REPEAT_RATE) == 0U) {
                    key_push_event((KeyId)i, KEY_EVENT_REPEAT);
                }
            }
        }
    }
}

bool key_pop_event(KeyEvent *event)
{
    if (g_tail == g_head) {
        return false;
    }
    *event = g_queue[g_tail];
    g_tail = (uint8_t)((g_tail + 1U) % KEY_QUEUE_SIZE);
    return true;
}

bool key_is_down(KeyId id)
{
    return g_keys[id].stable_down;
}
