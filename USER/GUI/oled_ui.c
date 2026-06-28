#include "oled_ui.h"

#include "ui_data.h"
#include "ui_msg.h"
#include "ui_nav.h"
#include "ui_page.h"
#include "ui_popup.h"
#include "music_fft.h"

static unsigned int OLED_UI_BuildSeed(void);

static unsigned int OLED_UI_BuildSeed(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    uint32_t seed = HAL_GetTick() ^ 0x5A17C3U;

    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK &&
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) == HAL_OK)
    {
        seed ^= ((uint32_t)sTime.Hours << 24);
        seed ^= ((uint32_t)sTime.Minutes << 16);
        seed ^= ((uint32_t)sTime.Seconds << 8);
        seed ^= ((uint32_t)sDate.Year << 4);
        seed ^= ((uint32_t)sDate.Month << 1);
        seed ^= (uint32_t)sDate.Date;
    }

    if (seed == 0U)
    {
        seed = 0x13572468U;
    }

    return (unsigned int)seed;
}

/*
 * UI 门面层。
 * 这一层只负责把“初始化、周期推进、渲染提交、事件分发”串起来，
 * 不直接维护页面切换状态机。
 */

void OLED_UI_Init(I2C_Bus_t *bus)
{
    // 重新进入 UI 前先清空运行时状态，避免页面和动画残留。
    UI_Data_ResetRuntime();

    OLED_Init(bus);
    UI_Popup_Init();

    // 上电后先进入 Boot 页，后续由导航层切到首页。
    g_ui.cur_page = PAGE_BOOT;
    g_ui.next_page = PAGE_HOME;
    g_ui.stage = UI_STAGE_NORMAL;

    // // 首帧电池状态直接从 GPIO 读取一次，后续再由消息层更新。
    // g_ui.battery.is_charging =
    //     (HAL_GPIO_ReadPin(BAT_CHG_GPIO_Port, BAT_CHG_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    // g_ui.battery.batterypower =
    //     (HAL_GPIO_ReadPin(POWER_IN_5V_GPIO_Port, POWER_IN_5V_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    // g_ui.battery.percent = 0U;

    // 随机数用于首页区域轮换和粒子效果。
    srand(OLED_UI_BuildSeed());
    UI_Nav_Init();
}

void OLED_UI_Update(void)
{
    MusicFFT_Process();

    // 所有 UI 周期逻辑都由导航层统一推进。
    UI_Nav_Update();
}

void OLED_UI_Render(void)
{
    u8g2_t *u8g2 = OLED_GetHandle();

    // 图层顺序固定为：页面底层 -> 粒子层 -> 弹窗层。
    u8g2_ClearBuffer(u8g2);
    if (UI_Nav_ShutdownActive())
    {
        UI_Nav_DrawShutdownOverlay(u8g2);
    }
    else
    {
        UI_Page_DrawBaseLayer(u8g2);
        UI_Nav_DrawParticleLayer(u8g2);
        UI_Popup_DrawTopLayer(u8g2);
    }

    u8g2_SetMaxClipWindow(u8g2);
    OLED_Update();
}

void OLED_UI_SetPage(UI_Page_t page)
{
    // 页面切换请求统一交给导航层，避免别处直接改状态机。
    UI_Nav_RequestPage(page);
}

void OLED_UI_OnEvent(UI_Event_t event)
{
    // 业务层只发事件，不关心弹窗文案和联动细节。
    UI_Msg_HandleEvent(event);
}

void OLED_UI_SetBattery(uint8_t percent, uint8_t charging)
{
    // 电池状态写入消息层，再由 UI 统一决定是否联动提示。
    UI_Msg_SetBattery(percent, charging);
}
