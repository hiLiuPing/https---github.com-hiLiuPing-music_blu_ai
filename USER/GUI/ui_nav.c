#include "ui_nav.h"

#include "ui_page.h"
#include "ui_widget.h"
#include "systemMonitor_app.h"
#include "log.h"
static void UI_Nav_CreateTimers(void);
static void UI_Nav_StartDynamicHold(void);
static void UI_Nav_StopRegionTimers(void);
static void UI_Nav_StartRegionTimers(void);
static void UI_Nav_ResetRegionDefaults(UI_Page_t page);
static void UI_Nav_EnterPage(UI_Page_t page);
static void UI_Nav_LeaveCurrentPage(void);
static void UI_Nav_UpdateClockLayer(void);
static void UI_Nav_ResetClockFallbackDefaults(void);
static void UI_Nav_TimerCallback(TimerHandle_t timer);
static void UI_Nav_RestartRegionTimer(uint8_t region);
static uint32_t UI_Nav_RandomRange(uint32_t min_ms, uint32_t max_ms);
static uint8_t UI_Nav_AllRegionsIdle(void);
static void UI_Nav_UpdateBoot(void);
static void UI_Nav_UpdateShutdown(void);
static void UI_Nav_UpdatePageTransition(void);
static void UI_Nav_UpdateDynamicPage(void);
static void UI_Nav_StartPageSwitch(UI_Page_t page);
static UI_Page_t UI_Nav_GetBootTargetPage(void);
static void UI_Nav_InitParticleLayer(void);
static void UI_Nav_ResetParticle(uint8_t idx, uint8_t random_y, UI_ParticleType_t type);
static UI_ParticleType_t UI_Nav_DetectParticleType(void);
static void UI_Nav_UpdateParticleLayer(void);
static uint8_t UI_Nav_ParticleLayerEnabled(void);
static uint32_t UI_Nav_AdvanceSteps(TickType_t *next_tick, TickType_t now, uint32_t frame_ms, uint32_t max_steps);

static uint32_t UI_Nav_AdvanceSteps(TickType_t *next_tick, TickType_t now, uint32_t frame_ms, uint32_t max_steps)
{
    uint32_t steps = 0U;
    TickType_t frame_ticks;

    if (next_tick == NULL || frame_ms == 0U)
    {
        return 0U;
    }

    frame_ticks = pdMS_TO_TICKS(frame_ms);
    if (frame_ticks == 0U)
    {
        frame_ticks = 1U;
    }

    while ((steps < max_steps) && ((int32_t)(now - *next_tick) >= 0))
    {
        *next_tick += frame_ticks;
        steps++;
    }

    return steps;
}

void UI_Nav_Init(void)
{
    UI_Nav_CreateTimers();
    UI_Nav_ResetRegionDefaults(PAGE_HOME);
    UI_Nav_InitParticleLayer();
    g_ui.boot_completed = 0U; // <-- 改为操作全局结构体变量
    g_ui_boot_start_tick = xTaskGetTickCount();
    g_ui_boot_next_frame_tick = g_ui_boot_start_tick;
    g_ui_boot_frame = 0U;
    g_ui_boot_variant = (uint8_t)((uint32_t)rand() % OLED_UI_BOOT_VARIANT_COUNT);

    if (g_ui.cur_page == PAGE_HOME)
    {
        UI_Nav_StartDynamicHold();
        (void)UI_Widget_StartClockRollPair(xTaskGetTickCount());
    }
}

void UI_Nav_Update(void)
{
    UI_Nav_UpdateShutdown();

    if (g_ui_shutdown.active)
    {
        return;
    }

    if (g_ui_page_trans.active)
    {
        UI_Nav_UpdatePageTransition();
        UI_Nav_UpdateParticleLayer();
        return;
    }

    if (g_ui.cur_page == PAGE_BOOT)
    {
        UI_Nav_UpdateBoot();
        return;
    }

    UI_Nav_UpdateClockLayer();

    if (g_ui.cur_page == PAGE_CLOCK)
    {
        /* 天气就绪后直接切到首页，不再需要滑条动画和 fallback 机制 */
        if (g_now_weather.icon != 0)
        {
            UI_Nav_RequestPage(PAGE_HOME);
            return;
        }
        if (UI_Page_CanUsePopups(g_ui.cur_page))
        {
            UI_Popup_Update();
        }
        return;
    }

    if (UI_Page_IsDynamic(g_ui.cur_page))
    {
        UI_Nav_UpdateDynamicPage();
    }

    if (UI_Page_CanUsePopups(g_ui.cur_page))
    {
        UI_Popup_Update();
    }

    UI_Nav_UpdateParticleLayer();
}

void UI_Nav_ShutdownStart(void)
{
    /* 如果还没有开机完成，直接拦截，不执行关机 */
    if (g_ui.boot_completed == 0U)
    {
        return;
    }
    g_ui_shutdown.active = 1U;
    g_ui_shutdown.canceling = 0U;
    g_ui_shutdown.committed = 0U;
    g_ui_shutdown.start_tick = xTaskGetTickCount();
    g_ui_shutdown.last_update_tick = g_ui_shutdown.start_tick;
    g_ui_shutdown.progress = 0U;
    g_ui_shutdown.display_progress = 0U;
}

void UI_Nav_ShutdownKeepAlive(void)
{
    if (g_ui_shutdown.active && !g_ui_shutdown.committed)
    {
        g_ui_shutdown.canceling = 0U;
    }
}

void UI_Nav_ShutdownCancel(void)
{
    if (g_ui_shutdown.active && !g_ui_shutdown.committed)
    {
        g_ui_shutdown.canceling = 1U;
    }
}

uint8_t UI_Nav_ShutdownActive(void)
{
    return g_ui_shutdown.active;
}

void UI_Nav_WakeUp(void)
{
    UI_Page_t target;

    if (g_ui_shutdown.active || g_ui_page_trans.active)
    {
        return;
    }

    target = (g_now_weather.icon == 0) ? PAGE_CLOCK : PAGE_HOME;

    if (target == g_ui.cur_page)
    {
        if (UI_Page_IsDynamic(g_ui.cur_page))
        {
            UI_Nav_StopRegionTimers();
            UI_Nav_ResetRegionDefaults(g_ui.cur_page);
            UI_Nav_StartDynamicHold();
            if (g_ui.cur_page == PAGE_HOME)
            {
                (void)UI_Widget_StartClockRollPair(xTaskGetTickCount());
                /* 唤醒后进入首页时刷新 OLED 空闲定时器，给轮播留出足够时间 */
                Monitor_Restart(MON_OLED_IDLE);
            }
        }
        else if (g_ui.cur_page == PAGE_CLOCK)
        {
            UI_Nav_ResetClockFallbackDefaults();
        }
    }
    else
    {
        UI_Nav_RequestPage(target);
    }
}

void UI_Nav_RequestPage(UI_Page_t page)
{
    if (page <= PAGE_BOOT || page >= PAGE_MAX)
    {
        return;
    }

    if (g_ui_shutdown.active)
    {
        return;
    }

    if (g_ui.cur_page == PAGE_BOOT || g_ui.cur_page == page || g_ui_page_trans.active)
    {
        return;
    }

    if (UI_Page_IsDynamic(g_ui.cur_page) && !UI_Nav_AllRegionsIdle())
    {
        return;
    }

    UI_Nav_StartPageSwitch(page);
}

void UI_Nav_DrawParticleLayer(u8g2_t *u8g2)
{
    uint8_t i;

    if (!UI_Nav_ParticleLayerEnabled())
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1);

    if (g_ui_particle_type == UI_PARTICLE_SNOW)
    {
        for (i = 0; i < g_ui_particle_active_count; i++)
        {
            UI_RainParticle_t *p = &g_ui_rain_particles[i];

            if (p->x < 0 || p->x >= OLED_UI_WIDTH || p->y < 0 || p->y >= OLED_UI_HEIGHT)
            {
                continue;
            }

            u8g2_DrawPixel(u8g2, p->x, p->y);
            if (p->x > 0) u8g2_DrawPixel(u8g2, p->x - 1, p->y);
            if (p->x < OLED_UI_WIDTH - 1) u8g2_DrawPixel(u8g2, p->x + 1, p->y);
            if (p->y > 0) u8g2_DrawPixel(u8g2, p->x, p->y - 1);
            if (p->y < OLED_UI_HEIGHT - 1) u8g2_DrawPixel(u8g2, p->x, p->y + 1);
        }
    }
    else
    {
        for (i = 0; i < g_ui_particle_active_count; i++)
        {
            UI_RainParticle_t *particle = &g_ui_rain_particles[i];
            int16_t draw_y = particle->y - particle->len + 1;

            if (particle->x < 0 || particle->x >= OLED_UI_WIDTH)
            {
                continue;
            }

            if (draw_y < 0)
            {
                draw_y = 0;
            }

            if (particle->y >= 0 && draw_y < OLED_UI_HEIGHT)
            {
                uint8_t visible_len = particle->len;

                if ((int16_t)(draw_y + visible_len) > OLED_UI_HEIGHT)
                {
                    visible_len = (uint8_t)(OLED_UI_HEIGHT - draw_y);
                }

                u8g2_DrawVLine(u8g2, (u8g2_uint_t)particle->x, (u8g2_uint_t)draw_y, visible_len);
            }
        }
    }
}

void UI_Nav_DrawShutdownOverlay(u8g2_t *u8g2)
{
    uint8_t p = g_ui_shutdown.display_progress;
    int16_t bar_w;
    int16_t bar_x;
    int16_t sweep_x;

    if (!g_ui_shutdown.active)
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawBox(u8g2, 0, 0, OLED_UI_WIDTH, OLED_UI_HEIGHT);

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawFrame(u8g2, 8, 4, 112, 24);

    bar_w = (int16_t)((uint32_t)(OLED_UI_WIDTH - 28U) * p / 100U);
    bar_x = 14;
    u8g2_DrawFrame(u8g2, bar_x, 24, OLED_UI_WIDTH - 2 * bar_x, 4);
    if (bar_w > 2)
    {
        u8g2_DrawBox(u8g2, bar_x + 1, 25, (u8g2_uint_t)(bar_w - 2), 2);
    }

    sweep_x = (int16_t)(10 + ((uint32_t)(OLED_UI_WIDTH - 20U) * p / 100U));
    u8g2_DrawVLine(u8g2, sweep_x, 6, 18);
    if (sweep_x > 1)
    {
        u8g2_DrawVLine(u8g2, sweep_x - 1, 7, 16);
    }
    if (sweep_x < OLED_UI_WIDTH - 1)
    {
        u8g2_DrawVLine(u8g2, sweep_x + 1, 7, 16);
    }

    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(u8g2, 38, 14, "POWER OFF");
    u8g2_DrawStr(u8g2, 38, 22, g_ui_shutdown.canceling ? "CANCEL" : "KEEP HOLD");

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawCircle(u8g2, 20, 16, 6, U8G2_DRAW_ALL);
    u8g2_DrawLine(u8g2, 20, 8, 20, 13);
    u8g2_DrawLine(u8g2, 17, 10, 23, 10);

    u8g2_SetDrawColor(u8g2, 1);
}

static void UI_Nav_CreateTimers(void)
{
    uint8_t i;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        if (g_ui_region_timers[i] == NULL)
        {
            g_ui_region_timers[i] = xTimerCreate("uiReg",
                                                 pdMS_TO_TICKS(UI_Nav_RandomRange(OLED_UI_REGION_TIMER_MIN_MS,
                                                                                  OLED_UI_REGION_TIMER_MAX_MS)),
                                                 pdTRUE,
                                                 (void *)(uintptr_t)i,
                                                 UI_Nav_TimerCallback);
        }
    }
}

static void UI_Nav_TimerCallback(TimerHandle_t timer)
{
    uint8_t idx = (uint8_t)(uintptr_t)pvTimerGetTimerID(timer);

    if (idx < OLED_UI_REGION_COUNT)
    {
        g_ui_region_refresh_req[idx] = 1U;
        UI_Nav_RestartRegionTimer(idx);
    }
}

static void UI_Nav_RestartRegionTimer(uint8_t region)
{
    uint32_t next_ms;

    if (region >= OLED_UI_REGION_COUNT || g_ui_region_timers[region] == NULL)
    {
        return;
    }

    next_ms = UI_Nav_RandomRange(OLED_UI_REGION_TIMER_MIN_MS, OLED_UI_REGION_TIMER_MAX_MS);
    (void)xTimerChangePeriod(g_ui_region_timers[region], pdMS_TO_TICKS(next_ms), 0);
}

static uint32_t UI_Nav_RandomRange(uint32_t min_ms, uint32_t max_ms)
{
    uint32_t span = (max_ms >= min_ms) ? (max_ms - min_ms + 1U) : 1U;
    return min_ms + ((uint32_t)rand() % span);
}

static void UI_Nav_StartDynamicHold(void)
{
    g_ui_dynamic_hold_until = xTaskGetTickCount() + pdMS_TO_TICKS(OLED_UI_HOME_DEFAULT_MS);
    g_ui_dynamic_timers_started = 0U;
}

static void UI_Nav_StopRegionTimers(void)
{
    uint8_t i;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        if (g_ui_region_timers[i] != NULL)
        {
            (void)xTimerStop(g_ui_region_timers[i], 0);
        }
        g_ui_region_refresh_req[i] = 0U;
        g_ui_regions[i].state = UI_REGION_IDLE;
        g_ui_regions[i].slide_offset = 0;
    }

    UI_Widget_StopClockRoll();
    g_ui_dynamic_timers_started = 0U;
}

static void UI_Nav_StartRegionTimers(void)
{
    uint8_t i;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        UI_Nav_RestartRegionTimer(i);
    }

    g_ui_dynamic_timers_started = 1U;
}

static void UI_Nav_ResetRegionDefaults(UI_Page_t page)
{
    uint8_t i;
    (void)page;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        g_ui_regions[i].state = UI_REGION_IDLE;
        g_ui_regions[i].current_draw = g_ui_home_default_draws[i];
        g_ui_regions[i].next_draw = NULL;
        g_ui_regions[i].slide_offset = 0;
        g_ui_regions[i].slide_dir = UI_SLIDE_DOWN;
        g_ui_regions[i].next_frame_tick = 0;
        g_ui_region_refresh_req[i] = 0U;
    }

    UI_Widget_StopClockRoll();
}

static void UI_Nav_ResetClockFallbackDefaults(void)
{
    TickType_t now = xTaskGetTickCount();

    UI_Widget_StopClockRoll();

    g_ui_regions[0].state = UI_REGION_IDLE;
    g_ui_regions[0].current_draw = UI_Widget_DrawRegionTempHumidSmall;
    g_ui_regions[0].next_draw = NULL;
    g_ui_regions[0].slide_offset = 0;
    g_ui_regions[0].slide_dir = UI_SLIDE_DOWN;
    g_ui_regions[0].next_frame_tick = now;
    g_ui_region_refresh_req[0] = 0U;

    g_ui_regions[1].state = UI_REGION_IDLE;
    g_ui_regions[1].current_draw = UI_Widget_DrawHomeClockMid;
    g_ui_regions[1].next_draw = NULL;
    g_ui_regions[1].slide_offset = 0;
    g_ui_regions[1].slide_dir = UI_SLIDE_DOWN;
    g_ui_regions[1].next_frame_tick = now;
    g_ui_region_refresh_req[1] = 0U;

    g_ui_regions[2].state = UI_REGION_IDLE;
    g_ui_regions[2].current_draw = UI_Widget_DrawHomeClockSec;
    g_ui_regions[2].next_draw = NULL;
    g_ui_regions[2].slide_offset = 0;
    g_ui_regions[2].slide_dir = UI_SLIDE_DOWN;
    g_ui_regions[2].next_frame_tick = now;
    g_ui_region_refresh_req[2] = 0U;

    g_ui_dynamic_timers_started = 0U;
}

static void UI_Nav_EnterPage(UI_Page_t page)
{
    g_ui.cur_page = page;
    g_ui.next_page = page;
    g_ui.stage = UI_STAGE_NORMAL;

    if (UI_Page_IsDynamic(page))
    {
        UI_Nav_ResetRegionDefaults(page);
        UI_Nav_StartDynamicHold();

        if (page == PAGE_HOME)
        {
            (void)UI_Widget_StartClockRollPair(xTaskGetTickCount());
            /* 进入首页时刷新 OLED 空闲定时器，给轮播留出足够时间 */
            Monitor_Restart(MON_OLED_IDLE);
        }
    }
    else if (page == PAGE_CLOCK)
    {
        UI_Nav_ResetClockFallbackDefaults();
    }
}

static void UI_Nav_LeaveCurrentPage(void)
{
    if (UI_Page_IsDynamic(g_ui.cur_page))
    {
        UI_Nav_StopRegionTimers();
    }
}

static uint8_t UI_Nav_AllRegionsIdle(void)
{
    uint8_t i;

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        if (g_ui_regions[i].state != UI_REGION_IDLE)
        {
            return 0U;
        }
    }

    return 1U;
}

static void UI_Nav_UpdateBoot(void)
{
    TickType_t now = xTaskGetTickCount();
    uint32_t steps;

    steps = UI_Nav_AdvanceSteps(&g_ui_boot_next_frame_tick, now, OLED_UI_BOOT_FRAME_MS, 8U);
    if (steps > 0U)
    {
        g_ui_boot_frame = (uint8_t)(g_ui_boot_frame + steps);
    }

    if ((uint32_t)((now - g_ui_boot_start_tick) * portTICK_PERIOD_MS) >= OLED_UI_BOOT_TIME_MS)
    {
        UI_Nav_StartPageSwitch(UI_Nav_GetBootTargetPage());
        g_ui.boot_completed = 1U; // <-- 开机时间到，置位
    }
}

static void UI_Nav_UpdateShutdown(void)
{
    TickType_t now;
    uint32_t elapsed_ms;
    uint32_t delta_ms;
    uint8_t target_progress;

    if (!g_ui_shutdown.active)
    {
        return;
    }

    if (g_ui_shutdown.committed)
    {
        return;
    }

    now = xTaskGetTickCount();
    delta_ms = (uint32_t)((now - g_ui_shutdown.last_update_tick) * portTICK_PERIOD_MS);
    g_ui_shutdown.last_update_tick = now;

    if (g_ui_shutdown.canceling)
    {
        if (delta_ms > 0U)
        {
            uint32_t rollback = (delta_ms * OLED_UI_SHUTDOWN_ROLLBACK_STEP) / 16U;
            if (rollback == 0U)
            {
                rollback = 1U;
            }

            if (g_ui_shutdown.display_progress > rollback)
            {
                g_ui_shutdown.display_progress = (uint8_t)(g_ui_shutdown.display_progress - rollback);
            }
            else
            {
                memset(&g_ui_shutdown, 0, sizeof(g_ui_shutdown));
            }
        }
        return;
    }

    elapsed_ms = (uint32_t)((now - g_ui_shutdown.start_tick) * portTICK_PERIOD_MS);
    if (elapsed_ms >= OLED_UI_SHUTDOWN_CONFIRM_MS)
    {
        g_ui_shutdown.progress = 100U;
        g_ui_shutdown.display_progress = 100U;
        g_ui_shutdown.committed = 1U;
        // System_PowerOff();
        music_send_cmd(CMD_SYSTEM_POWER_OFF);
        return;
    }

    target_progress = (uint8_t)((elapsed_ms * 100U) / OLED_UI_SHUTDOWN_CONFIRM_MS);
    g_ui_shutdown.progress = target_progress;
    if (g_ui_shutdown.display_progress < target_progress)
    {
        uint32_t advance = (delta_ms * 100U) / OLED_UI_SHUTDOWN_CONFIRM_MS;
        uint32_t next_progress;

        if (advance == 0U)
        {
            advance = 1U;
        }

        next_progress = (uint32_t)g_ui_shutdown.display_progress + advance;
        if (next_progress > target_progress)
        {
            next_progress = target_progress;
        }
        g_ui_shutdown.display_progress = (uint8_t)next_progress;
    }
    else
    {
        g_ui_shutdown.display_progress = target_progress;
    }
}

static void UI_Nav_StartPageSwitch(UI_Page_t page)
{
    if (g_ui_page_trans.active)
    {
        return;
    }

    UI_Nav_LeaveCurrentPage();

    g_ui.next_page = page;
    g_ui.stage = UI_STAGE_EXITING;

    g_ui_page_trans.active = 1U;
    g_ui_page_trans.from_page = g_ui.cur_page;
    g_ui_page_trans.to_page = page;
    g_ui_page_trans.offset = 0;
    g_ui_page_trans.next_frame_tick = xTaskGetTickCount();
}

static UI_Page_t UI_Nav_GetBootTargetPage(void)
{
    return (g_now_weather.icon == 0) ? PAGE_CLOCK : PAGE_HOME;
}


static UI_ParticleType_t UI_Nav_DetectParticleType(void)
{
    int icon = g_now_weather.icon;

    if (icon >= 300 && icon <= 302) return UI_PARTICLE_RAIN_LIGHT;
    if (icon >= 303 && icon <= 309) return UI_PARTICLE_RAIN_MODERATE;
    if (icon >= 310 && icon <= 399) return UI_PARTICLE_RAIN_HEAVY;
    if (icon >= 400 && icon <= 499) return UI_PARTICLE_SNOW;

    return UI_PARTICLE_NONE;
}

static void UI_Nav_ResetParticle(uint8_t idx, uint8_t random_y, UI_ParticleType_t type)
{
    UI_RainParticle_t *particle;

    if (idx >= OLED_UI_PARTICLE_MAX)
    {
        return;
    }

    particle = &g_ui_rain_particles[idx];
    particle->x = (int16_t)((uint32_t)rand() % OLED_UI_WIDTH);

    if (type == UI_PARTICLE_SNOW)
    {
        particle->y = random_y ? (int16_t)((uint32_t)rand() % OLED_UI_HEIGHT)
                               : (int16_t)(-((int32_t)rand() % 8));
        particle->len = 1;
        particle->speed = 1;
        particle->vx = (int8_t)(((uint32_t)rand() % 3U) - 1);
    }
    else
    {
        particle->y = random_y ? (int16_t)((uint32_t)rand() % OLED_UI_HEIGHT)
                               : (int16_t)(0 - ((uint32_t)rand() % 12U));
        if (type == UI_PARTICLE_RAIN_HEAVY)
        {
            particle->len = (uint8_t)(3U + ((uint32_t)rand() % 3U));
            particle->speed = (uint8_t)(2U + ((uint32_t)rand() % 2U));
        }
        else
        {
            particle->len = (uint8_t)(2U + ((uint32_t)rand() % 3U));
            particle->speed = (uint8_t)(1U + ((uint32_t)rand() % 2U));
        }
        particle->vx = (((uint32_t)rand() & 0x01U) != 0U) ? 1 : 0;
    }
}

static void UI_Nav_InitParticleLayer(void)
{
    uint8_t i;

    g_ui_particle_type = UI_Nav_DetectParticleType();

    switch (g_ui_particle_type)
    {
    case UI_PARTICLE_RAIN_LIGHT:    g_ui_particle_active_count = 6;  break;
    case UI_PARTICLE_RAIN_MODERATE: g_ui_particle_active_count = 12; break;
    case UI_PARTICLE_RAIN_HEAVY:    g_ui_particle_active_count = 24; break;
    case UI_PARTICLE_SNOW:          g_ui_particle_active_count = 18; break;
    default:                        g_ui_particle_active_count = 0;  break;
    }

    for (i = 0; i < g_ui_particle_active_count; i++)
    {
        UI_Nav_ResetParticle(i, 1U, g_ui_particle_type);
    }

    g_ui_rain_next_frame_tick = xTaskGetTickCount();
}

static uint8_t UI_Nav_ParticleLayerEnabled(void)
{
    if (g_ui_page_trans.active)
    {
        if (g_ui_page_trans.from_page == PAGE_BOOT || g_ui_page_trans.to_page == PAGE_BOOT)
        {
            return 0U;
        }
        return (g_ui_particle_type != UI_PARTICLE_NONE) ? 1U : 0U;
    }

    if (g_ui.cur_page == PAGE_BOOT)
    {
        return 0U;
    }

    return (g_ui_particle_type != UI_PARTICLE_NONE) ? 1U : 0U;
}

static void UI_Nav_UpdateParticleLayer(void)
{
    TickType_t now;
    uint8_t i;
    uint32_t steps;
    UI_ParticleType_t new_type = UI_Nav_DetectParticleType();

    if (new_type != g_ui_particle_type)
    {
        g_ui_particle_type = new_type;

        switch (g_ui_particle_type)
        {
        case UI_PARTICLE_RAIN_LIGHT:    g_ui_particle_active_count = 10; break;
        case UI_PARTICLE_RAIN_MODERATE: g_ui_particle_active_count = 20; break;
        case UI_PARTICLE_RAIN_HEAVY:    g_ui_particle_active_count = 24; break;
        case UI_PARTICLE_SNOW:          g_ui_particle_active_count = 20; break;
        default:                        g_ui_particle_active_count = 0; return;
        }

        for (i = 0; i < g_ui_particle_active_count; i++)
        {
            UI_Nav_ResetParticle(i, 1U, g_ui_particle_type);
        }

        g_ui_rain_next_frame_tick = xTaskGetTickCount();
        return;
    }

    if (!UI_Nav_ParticleLayerEnabled())
    {
        return;
    }

    now = xTaskGetTickCount();
    steps = UI_Nav_AdvanceSteps(&g_ui_rain_next_frame_tick,
                                now,
                                (g_ui_particle_type == UI_PARTICLE_SNOW) ? OLED_UI_SNOW_FRAME_MS : OLED_UI_RAIN_FRAME_MS,
                                8U);
    if (steps == 0U)
    {
        return;
    }

    for (i = 0; i < g_ui_particle_active_count; i++)
    {
        UI_RainParticle_t *particle = &g_ui_rain_particles[i];

        particle->y += (int16_t)(particle->speed * steps);
        particle->x += (int16_t)(particle->vx * (int8_t)steps);

        if (particle->x >= OLED_UI_WIDTH)
        {
            particle->x = 0;
        }
        else if (particle->x < 0)
        {
            particle->x = (int16_t)(OLED_UI_WIDTH - 1);
        }

        if (g_ui_particle_type == UI_PARTICLE_SNOW)
        {
            if (particle->y >= OLED_UI_HEIGHT)
            {
                UI_Nav_ResetParticle(i, 0U, g_ui_particle_type);
            }
        }
        else
        {
            if (particle->y - particle->len >= OLED_UI_HEIGHT)
            {
                UI_Nav_ResetParticle(i, 0U, g_ui_particle_type);
            }
        }
    }
}

static void UI_Nav_UpdatePageTransition(void)
{
    TickType_t now = xTaskGetTickCount();
    uint32_t steps = UI_Nav_AdvanceSteps(&g_ui_page_trans.next_frame_tick,
                                         now,
                                         OLED_UI_REGION_SLIDE_FRAME_MS,
                                         8U);

    if (steps == 0U)
    {
        return;
    }

    g_ui_page_trans.offset -= (int16_t)(OLED_UI_PAGE_SWITCH_STEP_PX * steps);

    if (g_ui_page_trans.offset <= -OLED_UI_WIDTH)
    {
        UI_Page_t page = g_ui_page_trans.to_page;

        memset(&g_ui_page_trans, 0, sizeof(g_ui_page_trans));
        UI_Nav_EnterPage(page);
    }
}

static void UI_Nav_UpdateDynamicPage(void)
{
    TickType_t now = xTaskGetTickCount();
    uint8_t i;

    if (!g_ui_dynamic_timers_started)
    {
        if ((int32_t)(now - g_ui_dynamic_hold_until) >= 0)
        {
            UI_Nav_StartRegionTimers();
        }
        return;
    }

    for (i = 0; i < OLED_UI_REGION_COUNT; i++)
    {
        UI_Region_t *region = &g_ui_regions[i];

        if (g_ui_region_refresh_req[i])
        {
            g_ui_region_refresh_req[i] = 0U;

            if (region->state == UI_REGION_IDLE)
            {
                uint8_t count = g_ui_region_lib_counts[i];
                uint8_t index = (uint8_t)((uint32_t)rand() % count);
                UI_DrawFunc_t candidate = g_ui_region_libs[i][index];

                if (candidate == region->current_draw)
                {
                    continue;
                }

                // if ((i == 1U) &&
                //     (g_ui_regions[0].current_draw == UI_Widget_DrawRegionWeather ||
                //      g_ui_regions[0].current_draw == UI_Widget_DrawRegionHighTemp ||
                //      g_ui_regions[0].current_draw == UI_Widget_DrawRegionLowTemp ||
                //      g_ui_regions[0].current_draw == UI_Widget_DrawRegionPM25 ||
                //      g_ui_regions[0].current_draw == UI_Widget_DrawRegionHumidity ||
                //      g_ui_regions[0].current_draw == UI_Widget_DrawRegionIcon))
                // {
                //     index = (uint8_t)((uint32_t)rand() % count);
                //     candidate = g_ui_region_libs[i][index];
                //     if (candidate == region->current_draw)
                //     {
                //         continue;
                //     }
                // }

                region->next_draw = candidate;
                region->slide_dir = UI_SLIDE_DOWN;
                region->slide_offset = 0;
                region->state = UI_REGION_SLIDING;
                region->next_frame_tick = now;
            }
        }

        if (region->state == UI_REGION_SLIDING)
        {
            uint32_t steps = UI_Nav_AdvanceSteps(&region->next_frame_tick, now, OLED_UI_REGION_SLIDE_FRAME_MS, 8U);
            if (steps > 0U)
            {
                region->slide_offset += (int16_t)(steps * OLED_UI_REGION_SLIDE_STEP_PX);
            }

            if (region->slide_offset >= OLED_UI_HEIGHT)
            {
                region->current_draw = region->next_draw;
                region->next_draw = NULL;
                region->slide_offset = 0;
                region->state = UI_REGION_IDLE;
            }
        }
    }
}

static void UI_Nav_UpdateClockLayer(void)
{
    if ((g_ui.cur_page == PAGE_HOME || g_ui.cur_page == PAGE_CLOCK) && !g_ui_page_trans.active)
    {
        UI_Widget_UpdateClockRoll();
    }
}
