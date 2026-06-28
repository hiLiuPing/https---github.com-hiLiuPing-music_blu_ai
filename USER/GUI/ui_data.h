#ifndef __UI_DATA_H__
#define __UI_DATA_H__

/*
 * UI 运行时数据层。
 * 所有页面、区域、弹窗和动画状态机共享的数据都集中在这里定义。
 */

#include "oled_ui.h"

#include "data_app.h"
#include "icons.h"
#include "music_app.h"
#include "oled_clock_font.h"
#include "oled_popup.h"
#include "oled_u8g2.h"
#include "systemMonitor.h"
#include "weather_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OLED 和首页区域配置。 */
#define OLED_UI_WIDTH                  128U
#define OLED_UI_HEIGHT                 32U
#define OLED_UI_REGION_COUNT           3U

/* 首页默认停留和区域切换节拍。 */
#define OLED_UI_HOME_DEFAULT_MS        3000U
#define OLED_UI_REGION_TIMER_MIN_MS    5000U
#define OLED_UI_REGION_TIMER_MAX_MS    8000U
#define OLED_UI_REGION_SLIDE_STEP_PX   2
#define OLED_UI_REGION_SLIDE_FRAME_MS  12U

/* 页面切换、Boot 和粒子动画节拍。 */
#define OLED_UI_PAGE_SWITCH_STEP_PX    8
#define OLED_UI_BOOT_TIME_MS           2200U
#define OLED_UI_BOOT_FRAME_MS          80U
#define OLED_UI_BOOT_VARIANT_COUNT     3U

#define OLED_UI_SHUTDOWN_CONFIRM_MS    2000U
#define OLED_UI_SHUTDOWN_ROLLBACK_STEP 8U

#define OLED_UI_PARTICLE_MAX           24U
#define OLED_UI_RAIN_FRAME_MS          40U
#define OLED_UI_SNOW_FRAME_MS          80U

typedef enum {
    UI_PARTICLE_NONE = 0,
    UI_PARTICLE_RAIN_LIGHT,
    UI_PARTICLE_RAIN_MODERATE,
    UI_PARTICLE_RAIN_HEAVY,
    UI_PARTICLE_SNOW,
} UI_ParticleType_t;
#define OLED_UI_CLOCK_ROLL_STEP_PX     2
#define OLED_UI_CLOCK_ROLL_FRAME_MS    12U

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t w;
    uint8_t h;
} UI_Rect_t;

typedef struct {
    UI_RegionState_t state;
    UI_DrawFunc_t current_draw;
    UI_DrawFunc_t next_draw;
    int16_t slide_offset;
    UI_SlideDir_t slide_dir;
    TickType_t next_frame_tick;
} UI_Region_t;

typedef struct {
    uint8_t active;
    UI_Page_t from_page;
    UI_Page_t to_page;
    int16_t offset;
    TickType_t next_frame_tick;
} UI_PageTransition_t;

typedef struct {
    UI_Page_t page;
    UI_ExtPageMode_t mode;
} UI_ExtPageConfig_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t len;
    uint8_t speed;
    int8_t vx;
} UI_RainParticle_t;

typedef struct {
    uint8_t cur;
    uint8_t next;
    uint8_t anim;
    int16_t offset;
    UI_SlideDir_t dir;
    TickType_t next_frame_tick;
} UI_ClockDigit_t;

typedef struct {
    uint8_t ready;
    uint8_t last_hour;
    uint8_t last_min;
    uint8_t last_sec;
    UI_ClockDigit_t mid_digits[4];
    UI_ClockDigit_t sec_digits[2];
} UI_ClockRoll_t;

typedef struct {
    uint8_t active;
    uint8_t canceling;
    uint8_t committed;
    TickType_t start_tick;
    TickType_t last_update_tick;
    uint8_t progress;
    uint8_t display_progress;
} UI_ShutdownAnim_t;

extern volatile  UI_Global_t g_ui;

extern const UI_Rect_t g_ui_home_regions[OLED_UI_REGION_COUNT];

extern UI_Region_t g_ui_regions[OLED_UI_REGION_COUNT];
extern volatile uint8_t g_ui_region_refresh_req[OLED_UI_REGION_COUNT];
extern TimerHandle_t g_ui_region_timers[OLED_UI_REGION_COUNT];
extern TickType_t g_ui_dynamic_hold_until;
extern uint8_t g_ui_dynamic_timers_started;

extern UI_PageTransition_t g_ui_page_trans;
extern TickType_t g_ui_boot_start_tick;
extern TickType_t g_ui_boot_next_frame_tick;
extern uint8_t g_ui_boot_frame;
extern uint8_t g_ui_boot_variant;
extern uint16_t g_ui_spectrum_tick;
extern UI_RainParticle_t g_ui_rain_particles[OLED_UI_PARTICLE_MAX];
extern UI_ParticleType_t g_ui_particle_type;
extern uint8_t g_ui_particle_active_count;
extern TickType_t g_ui_rain_next_frame_tick;
extern UI_ClockRoll_t g_ui_clock_roll;
extern UI_ShutdownAnim_t g_ui_shutdown;

/* 复位 UI 运行时状态。 */
void UI_Data_ResetRuntime(void);

#endif
