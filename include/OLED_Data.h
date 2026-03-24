#ifndef __OLED_DATA_H
#define __OLED_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if defined(OLED_CHARSET_UTF8) && defined(OLED_CHARSET_GB2312)
#error "OLED_CHARSET_UTF8 and OLED_CHARSET_GB2312 can not be enabled at the same time."
#endif

#if !defined(OLED_CHARSET_UTF8) && !defined(OLED_CHARSET_GB2312)
#define OLED_CHARSET_UTF8
#endif

#ifdef OLED_CHARSET_UTF8
#define OLED_CHINESE_INDEX_MAX_LEN 5
#else
#define OLED_CHINESE_INDEX_MAX_LEN 3
#endif

typedef struct
{
    char Index[OLED_CHINESE_INDEX_MAX_LEN];
    uint8_t Data[32];
} OLED_Chinese_t;

/* 兼容旧版工程中的类型名 */
typedef OLED_Chinese_t ChineseCell_t;

/* ASCII 字模 */
extern const uint8_t OLED_F8x16[][16];
extern const uint8_t OLED_F6x8[][6];

/* 汉字字模 */
extern const OLED_Chinese_t OLED_CF16x16[];

/* 图像 */
extern const uint8_t Diode[];

/* 查询接口：未找到时返回默认占位字模（数组末尾的空字符串项） */
const OLED_Chinese_t *OLED_FindChineseGlyph(const char *Index);

#ifdef __cplusplus
}
#endif

#endif
