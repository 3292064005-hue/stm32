#include "display.h"
#include "app_config.h"
#include "stm32f1xx_hal.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

static uint8_t g_oled_buffer[OLED_BUFFER_SIZE];
static uint8_t g_prev_buffer[OLED_BUFFER_SIZE];

static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00}, {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06}, {0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40}, {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00}, {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, {0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00}, {0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00}, {0x08,0x08,0x2A,0x1C,0x08}, {0x08,0x1C,0x2A,0x08,0x08}
};

static HAL_StatusTypeDef oled_write_cmds(const uint8_t *cmds, uint8_t count)
{
    uint8_t packet[17];
    if (count > 16U) {
        count = 16U;
    }
    packet[0] = 0x00;
    memcpy(&packet[1], cmds, count);
    return HAL_I2C_Master_Transmit(&OLED_I2C_HANDLE, OLED_I2C_ADDRESS, packet, (uint16_t)(count + 1U), OLED_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef oled_write_data_chunk(const uint8_t *data, uint8_t len)
{
    uint8_t packet[1 + OLED_I2C_CHUNK_BYTES];
    if (len > OLED_I2C_CHUNK_BYTES) {
        len = OLED_I2C_CHUNK_BYTES;
    }
    packet[0] = 0x40;
    memcpy(&packet[1], data, len);
    return HAL_I2C_Master_Transmit(&OLED_I2C_HANDLE, OLED_I2C_ADDRESS, packet, (uint16_t)(len + 1U), OLED_I2C_TIMEOUT_MS);
}

static void oled_set_window(uint8_t page, uint8_t col_start, uint8_t col_end)
{
    uint8_t cmds[] = {0x21, col_start, col_end, 0x22, page, page};
    (void)oled_write_cmds(cmds, sizeof(cmds));
}

void display_init(void)
{
#if OLED_RESET_ENABLED
    HAL_GPIO_WritePin(OLED_RESET_GPIO_Port, OLED_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(OLED_RESET_GPIO_Port, OLED_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
#else
    HAL_Delay(40);
#endif

    const uint8_t init_cmds[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0x9F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0x2E,
        0xAF
    };

    uint8_t idx = 0U;
    while (idx < sizeof(init_cmds)) {
        uint8_t batch = 1U;
        if (idx + 1U < sizeof(init_cmds)) {
            uint8_t cmd = init_cmds[idx];
            if (cmd == 0xD5 || cmd == 0xA8 || cmd == 0xD3 || cmd == 0x8D || cmd == 0x20 ||
                cmd == 0xDA || cmd == 0x81 || cmd == 0xD9 || cmd == 0xDB) {
                batch = 2U;
            }
        }
        (void)oled_write_cmds(&init_cmds[idx], batch);
        idx = (uint8_t)(idx + batch);
    }

    memset(g_prev_buffer, 0xFF, sizeof(g_prev_buffer));
    display_clear();
    display_present();
}

void display_set_contrast(uint8_t value)
{
    uint8_t cmds[] = {0x81, value};
    (void)oled_write_cmds(cmds, sizeof(cmds));
}

void display_sleep(bool sleep)
{
    uint8_t cmd = sleep ? 0xAE : 0xAF;
    (void)oled_write_cmds(&cmd, 1U);
}

void display_clear(void)
{
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
}

void display_present(void)
{
    for (uint8_t page = 0; page < 8U; ++page) {
        uint16_t base = (uint16_t)page * OLED_WIDTH;
        uint8_t x = 0U;
        while (x < OLED_WIDTH) {
            while (x < OLED_WIDTH && g_oled_buffer[base + x] == g_prev_buffer[base + x]) {
                x++;
            }
            if (x >= OLED_WIDTH) {
                break;
            }

            uint8_t start = x;
            while (x < OLED_WIDTH && g_oled_buffer[base + x] != g_prev_buffer[base + x]) {
                x++;
            }
            uint8_t end = (uint8_t)(x - 1U);

            oled_set_window(page, start, end);
            uint16_t offset = base + start;
            uint8_t remaining = (uint8_t)(end - start + 1U);
            while (remaining > 0U) {
                uint8_t chunk = remaining > OLED_I2C_CHUNK_BYTES ? OLED_I2C_CHUNK_BYTES : remaining;
                (void)oled_write_data_chunk(&g_oled_buffer[offset], chunk);
                memcpy(&g_prev_buffer[offset], &g_oled_buffer[offset], chunk);
                offset += chunk;
                remaining = (uint8_t)(remaining - chunk);
            }
        }
    }
}

void display_draw_pixel(int16_t x, int16_t y, bool color)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }
    uint8_t page = (uint8_t)(y / 8);
    uint16_t idx = (uint16_t)x + (uint16_t)page * OLED_WIDTH;
    uint8_t mask = (uint8_t)(1U << (y & 7));
    if (color) {
        g_oled_buffer[idx] |= mask;
    } else {
        g_oled_buffer[idx] &= (uint8_t)~mask;
    }
}

void display_draw_hline(int16_t x, int16_t y, int16_t w, bool color)
{
    for (int16_t i = 0; i < w; ++i) {
        display_draw_pixel(x + i, y, color);
    }
}

void display_draw_vline(int16_t x, int16_t y, int16_t h, bool color)
{
    for (int16_t i = 0; i < h; ++i) {
        display_draw_pixel(x, y + i, color);
    }
}

void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    if (w <= 0 || h <= 0) return;
    display_draw_hline(x, y, w, color);
    display_draw_hline(x, y + h - 1, w, color);
    display_draw_vline(x, y, h, color);
    display_draw_vline(x + w - 1, y, h, color);
}

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    for (int16_t yy = 0; yy < h; ++yy) {
        display_draw_hline(x, y + yy, w, color);
    }
}

void display_draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    display_draw_hline(x + 2, y, w - 4, color);
    display_draw_hline(x + 2, y + h - 1, w - 4, color);
    display_draw_vline(x, y + 2, h - 4, color);
    display_draw_vline(x + w - 1, y + 2, h - 4, color);
    display_draw_pixel(x + 1, y + 1, color);
    display_draw_pixel(x + w - 2, y + 1, color);
    display_draw_pixel(x + 1, y + h - 2, color);
    display_draw_pixel(x + w - 2, y + h - 2, color);
}

void display_fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    display_fill_rect(x + 1, y, w - 2, h, color);
    display_fill_rect(x, y + 1, w, h - 2, color);
}

void display_draw_circle(int16_t x0, int16_t y0, int16_t r, bool color)
{
    int16_t x = r, y = 0;
    int16_t err = 0;
    while (x >= y) {
        display_draw_pixel(x0 + x, y0 + y, color);
        display_draw_pixel(x0 + y, y0 + x, color);
        display_draw_pixel(x0 - y, y0 + x, color);
        display_draw_pixel(x0 - x, y0 + y, color);
        display_draw_pixel(x0 - x, y0 - y, color);
        display_draw_pixel(x0 - y, y0 - x, color);
        display_draw_pixel(x0 + y, y0 - x, color);
        display_draw_pixel(x0 + x, y0 - y, color);
        y++;
        if (err <= 0) err += 2 * y + 1;
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

void display_fill_circle(int16_t x0, int16_t y0, int16_t r, bool color)
{
    for (int16_t y = -r; y <= r; ++y) {
        for (int16_t x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                display_draw_pixel(x0 + x, y0 + y, color);
            }
        }
    }
}

void display_draw_char_5x7(int16_t x, int16_t y, char c, bool color)
{
    if (c < 32 || c > 127) {
        c = '?';
    }
    const uint8_t *glyph = font5x7[(uint8_t)c - 32U];
    for (uint8_t col = 0; col < 5; ++col) {
        for (uint8_t row = 0; row < 7; ++row) {
            bool on = ((glyph[col] >> row) & 0x01U) != 0U;
            if (on) {
                display_draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void display_draw_text_5x7(int16_t x, int16_t y, const char *text, bool color)
{
    while (*text) {
        display_draw_char_5x7(x, y, *text++, color);
        x += 6;
    }
}

void display_draw_text_centered_5x7(int16_t x, int16_t y, int16_t w, const char *text, bool color)
{
    int16_t len = 0;
    const char *p = text;
    while (*p++) len++;
    int16_t width = len * 6 - (len ? 1 : 0);
    display_draw_text_5x7(x + (w - width) / 2, y, text, color);
}

static void draw_seg(int16_t x, int16_t y, int16_t w, int16_t h, bool vertical, bool color)
{
    if (vertical) display_fill_round_rect(x, y, h, w, color);
    else display_fill_round_rect(x, y, w, h, color);
}

void display_draw_big_digit(int16_t x, int16_t y, uint8_t digit, uint8_t scale, bool color)
{
    static const uint8_t seg_map[10] = {
        0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
        0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011
    };
    uint8_t s = seg_map[digit % 10U];
    int16_t thick = scale;
    int16_t len = scale * 4;

    if (s & 0b1000000) draw_seg(x + thick, y, len, thick, false, color);
    if (s & 0b0100000) draw_seg(x + len + thick, y + thick, len, thick, true, color);
    if (s & 0b0010000) draw_seg(x + len + thick, y + len + 2 * thick, len, thick, true, color);
    if (s & 0b0001000) draw_seg(x + thick, y + 2 * len + 2 * thick, len, thick, false, color);
    if (s & 0b0000100) draw_seg(x, y + len + 2 * thick, len, thick, true, color);
    if (s & 0b0000010) draw_seg(x, y + thick, len, thick, true, color);
    if (s & 0b0000001) draw_seg(x + thick, y + len + thick, len, thick, false, color);
}

void display_draw_big_colon(int16_t x, int16_t y, uint8_t scale, bool color, bool on)
{
    if (!on) return;
    display_fill_circle(x, y + scale * 3, scale, color);
    display_fill_circle(x, y + scale * 8, scale, color);
}

void display_draw_battery_icon(int16_t x, int16_t y, uint8_t percent, bool charging, bool invert)
{
    bool c = !invert;
    display_draw_rect(x, y, 16, 8, c);
    display_fill_rect(x + 16, y + 2, 2, 4, c);
    uint8_t fill = (uint8_t)((percent * 12U) / 100U);
    display_fill_rect(x + 2, y + 2, fill, 4, c);
    if (charging) {
        display_draw_pixel(x + 7, y + 1, c);
        display_draw_pixel(x + 6, y + 3, c);
        display_draw_pixel(x + 8, y + 3, c);
        display_draw_pixel(x + 7, y + 5, c);
    }
}

void display_draw_step_icon(int16_t x, int16_t y, bool invert)
{
    bool c = !invert;
    display_fill_circle(x + 5, y + 4, 3, c);
    display_fill_circle(x + 10, y + 10, 4, c);
}

void display_draw_alarm_icon(int16_t x, int16_t y, bool invert)
{
    bool c = !invert;
    display_draw_circle(x + 8, y + 8, 5, c);
    display_draw_vline(x + 8, y + 8, 3, c);
    display_draw_hline(x + 8, y + 8, 3, c);
    display_draw_circle(x + 3, y + 2, 2, c);
    display_draw_circle(x + 13, y + 2, 2, c);
}

void display_draw_timer_icon(int16_t x, int16_t y, bool invert)
{
    bool c = !invert;
    display_draw_circle(x + 8, y + 9, 6, c);
    display_draw_rect(x + 6, y + 1, 4, 2, c);
    display_draw_vline(x + 8, y + 9, 3, c);
    display_draw_hline(x + 8, y + 9, 2, c);
}

void display_draw_gear_icon(int16_t x, int16_t y, bool invert)
{
    bool c = !invert;
    display_draw_circle(x + 8, y + 8, 5, c);
    display_fill_circle(x + 8, y + 8, 2, c);
    display_draw_vline(x + 8, y, 3, c);
    display_draw_vline(x + 8, y + 13, 3, c);
    display_draw_hline(x, y + 8, 3, c);
    display_draw_hline(x + 13, y + 8, 3, c);
}

void display_draw_info_icon(int16_t x, int16_t y, bool invert)
{
    bool c = !invert;
    display_draw_circle(x + 8, y + 8, 7, c);
    display_fill_circle(x + 8, y + 4, 1, c);
    display_draw_vline(x + 8, y + 7, 5, c);
}

void display_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, bool invert)
{
    bool c = !invert;
    display_draw_round_rect(x, y, w, h, c);
    int16_t fill = (int16_t)(((w - 4) * percent) / 100U);
    if (fill > 0) {
        display_fill_round_rect(x + 2, y + 2, fill, h - 4, c);
    }
}
