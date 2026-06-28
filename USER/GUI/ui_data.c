#include "ui_data.h"

/*
 * UI 运行时数据定义与复位。
 * 这里不画任何图，只定义全局状态并提供统一清零入口。
 */

volatile UI_Global_t g_ui = {
    .sys_running = 1
};

const UI_Rect_t g_ui_home_regions[OLED_UI_REGION_COUNT] = {
    {0, 0, 32, 32},
    {32, 0, 64, 32},
    {96, 0, 32, 32},
};

UI_Region_t g_ui_regions[OLED_UI_REGION_COUNT];
volatile uint8_t g_ui_region_refresh_req[OLED_UI_REGION_COUNT];
TimerHandle_t g_ui_region_timers[OLED_UI_REGION_COUNT];
TickType_t g_ui_dynamic_hold_until;
uint8_t g_ui_dynamic_timers_started;

UI_PageTransition_t g_ui_page_trans;
TickType_t g_ui_boot_start_tick;
TickType_t g_ui_boot_next_frame_tick;
uint8_t g_ui_boot_frame;
uint8_t g_ui_boot_variant;
uint16_t g_ui_spectrum_tick;
UI_RainParticle_t g_ui_rain_particles[OLED_UI_PARTICLE_MAX];
UI_ParticleType_t g_ui_particle_type;
uint8_t g_ui_particle_active_count;
TickType_t g_ui_rain_next_frame_tick;
UI_ClockRoll_t g_ui_clock_roll;
UI_ShutdownAnim_t g_ui_shutdown;

/**
 * @brief  复位全部 UI 运行时状态
 * @retval None
 */
void UI_Data_ResetRuntime(void)
{
    // memset(&g_ui, 0, sizeof(g_ui));
    memset(g_ui_regions, 0, sizeof(g_ui_regions));
    memset((void *)g_ui_region_refresh_req, 0, sizeof(g_ui_region_refresh_req));
    memset(g_ui_region_timers, 0, sizeof(g_ui_region_timers));
    memset(&g_ui_page_trans, 0, sizeof(g_ui_page_trans));
    memset(g_ui_rain_particles, 0, sizeof(g_ui_rain_particles));
    memset(&g_ui_clock_roll, 0, sizeof(g_ui_clock_roll));
    memset(&g_ui_shutdown, 0, sizeof(g_ui_shutdown));

    g_ui_dynamic_hold_until = 0;
    g_ui_dynamic_timers_started = 0U;
    g_ui_boot_start_tick = 0;
    g_ui_boot_next_frame_tick = 0;
    g_ui_boot_frame = 0U;
    g_ui_boot_variant = 0U;
    g_ui_spectrum_tick = 0U;
    g_ui_rain_next_frame_tick = 0;
    g_ui_particle_type = UI_PARTICLE_NONE;
    g_ui_particle_active_count = 0U;
    g_ui.oled_showing = 1U;
}
