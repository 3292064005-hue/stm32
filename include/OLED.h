#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stddef.h>
#include "OLED_Data.h"

/* 屏幕参数 */
#define OLED_WIDTH              128U
#define OLED_HEIGHT             64U
#define OLED_PAGE_COUNT         8U

/* 常用参数 */
#define OLED_8X16               8U
#define OLED_6X8                6U

#define OLED_UNFILLED           0U
#define OLED_FILLED             1U

/* SSD1306 常见 7bit 地址为 0x3C，HAL 发送时需左移 1 位 */
#define OLED_I2C_ADDR_7BIT      0x3CU
#define OLED_I2C_ADDR_8BIT      (OLED_I2C_ADDR_7BIT << 1)

/* 显存 */
extern uint8_t OLED_DisplayBuf[OLED_PAGE_COUNT][OLED_WIDTH];

/* 基础通信与初始化 */
void OLED_Init(void);
void OLED_SetCursor(uint8_t Page, uint8_t X);
void OLED_WriteCommand(uint8_t Command);
void OLED_WriteData(const uint8_t *Data, uint8_t Count);

/* 显示控制 */
void OLED_DisplayOn(void);
void OLED_DisplayOff(void);
void OLED_SetContrast(uint8_t Contrast);
void OLED_SetInverse(uint8_t IsInverse);

/* 刷新与缓冲区操作 */
void OLED_Update(void);
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void OLED_Clear(void);
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void OLED_Fill(void);
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/* 文本显示 */
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);
void OLED_ShowString(int16_t X, int16_t Y, const char *String, uint8_t FontSize);
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, const char *format, ...);

/* 图像与绘图 */
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);
void OLED_DrawPoint(int16_t X, int16_t Y);
uint8_t OLED_GetPoint(int16_t X, int16_t Y);
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

#ifdef __cplusplus
}
#endif

#endif
