#ifndef __UI_WIDGET_H__
#define __UI_WIDGET_H__

/*
 * UI Widget 组件库。
 * 页面通过这里暴露的绘制函数拼装首页区域和其它局部模块。
 */

#include "ui_data.h"

extern const UI_DrawFunc_t g_ui_region0_lib[];
extern const UI_DrawFunc_t g_ui_region1_lib[];
extern const UI_DrawFunc_t g_ui_region2_lib[];
extern const UI_DrawFunc_t * const g_ui_region_libs[OLED_UI_REGION_COUNT];
extern const uint8_t g_ui_region_lib_counts[OLED_UI_REGION_COUNT];
extern const UI_DrawFunc_t g_ui_home_default_draws[OLED_UI_REGION_COUNT];

void UI_Widget_UpdateClockRoll(void);
void UI_Widget_StopClockRoll(void);
uint8_t UI_Widget_StartClockRollPair(TickType_t now);

void UI_Widget_DrawHomeClockMid(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawHomeClockSec(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawHomeDefaultLeft(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawHomeDefaultMiddle(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawHomeDefaultRight(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionWeather(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionTemp(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionMusic(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionAir(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionBattery(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionPower(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionSystem(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionHighTemp(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionLowTemp(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionPM25(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionHumidity(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionIcon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionTempHumid(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionWeatherPM(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionIcon3(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionIcon1_2(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawStatusBattery(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t pct, uint8_t charging);
void UI_Widget_DrawTinySpectrum(u8g2_t *u8g2, int16_t x, int16_t y);
void UI_Widget_DrawCenteredStr(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, const char *str);
void UI_Widget_DrawCenteredStrUTF8(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, const char *str);
void UI_Widget_DrawWeatherStr(u8g2_t *u8g2, int16_t x, int16_t y, const char *str);
void UI_Widget_DrawCenteredWeatherStr(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, const char *str);
void UI_Widget_DrawRegionHumidityNow(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionTempNow(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);
void UI_Widget_DrawRegionTempHumidSmall(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);

#endif
