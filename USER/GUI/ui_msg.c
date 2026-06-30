#include "ui_msg.h"

#include "data_app.h"
#include "oled_ui.h"
#include "ui_nav.h"
#include "ui_page.h"
#include "ui_popup.h"

/*
 * UI 事件映射层。
 * 把业务事件翻译成具体的弹窗内容和页面动作。
 */

void UI_Msg_SetBattery(uint8_t percent, uint8_t charging)
{
    g_ui.battery.percent = (percent > 100U) ? 100U : percent;
    g_ui.battery.is_charging = charging ? 1U : 0U;

    if (charging && UI_Page_CanUsePopups(g_ui.cur_page))
    {
        UI_Popup_Show(Draw_BatteryFullPopup, "", 3000);
    }
}

void UI_Msg_HandleEvent(UI_Event_t event)
{
    if (g_ui.cur_page == PAGE_BOOT)
    {
        /* 开机期间允许电池/电源事件通过，事件队列中的弹窗会在开机完成后显示。 */
        switch (event)
        {
        case UI_EVT_BATTERY_LOW:
        case UI_EVT_BATTERY_FULL:
        case UI_EVT_BATTERY_CHARGING:
        case UI_EVT_POWEROUT:
        case UI_EVT_POWERIN:
        case UI_EVT_WEATHER_TIME_SYNC:
        case UI_EVT_WEATHER_TIME_SYNC_OK:
            break;
        default:
            return;
        }
    }

    switch (event)
    {
    case UI_EVT_PLAY:
        UI_Popup_Show(Draw_SystemMsgPopup, "PLAY", 500);
        break;
    case UI_EVT_PAUSE:
        UI_Popup_Show(Draw_SystemMsgPopup, "PAUSE", 500);
        break;
    case UI_EVT_STOP:
        OLED_UI_SetPage(PAGE_HOME);
        UI_Popup_Show(Draw_SystemMsgPopup, "STOPPED", 500);
        break;
    case UI_EVT_NEXT:
        UI_Popup_Show(Draw_SystemMsgPopup, "NEXT >>", 500);
        // if (g_ui.cur_page == PAGE_HOME)
        // {
        //     OLED_UI_SetPage(PAGE_PAGE1);
        // }
        break;
    case UI_EVT_PREV:
        UI_Popup_Show(Draw_SystemMsgPopup, "<< PREV", 500);
        // if (g_ui.cur_page == PAGE_HOME)
        // {
        //     OLED_UI_SetPage(PAGE_PAGE1);
        // }
        break;
    case UI_EVT_VOL_UP:
        UI_Popup_Show(Draw_SystemMsgPopup, "VOL +", 500);
        break;
    case UI_EVT_VOL_DOWN:
        UI_Popup_Show(Draw_SystemMsgPopup, "VOL -", 500);
        break;
    case UI_EVT_BATTERY_LOW:
        UI_Popup_Show(Draw_SystemMsgPopup, "LOW BATTERY", 500);
        break;
    case UI_EVT_TEMPERATURE_HIGH:
        UI_Popup_Show(Draw_SystemMsgPopup, "TEMP HIGH", 500);
        break;
    case UI_EVT_TEMPERATURE_NORMAL:
        UI_Popup_Show(Draw_SystemMsgPopup, "TEMP NORMAL", 500);
        break;
    case UI_EVT_BLUETOOTH_CONNECTED:
        UI_Popup_Show(Draw_SystemMsgPopup, "BT CONNECTED", 500);
        break;
    case UI_EVT_BLUETOOTH_DISCONNECTED:
        UI_Popup_Show(Draw_SystemMsgPopup, "BT DISCONNECTED", 500);
        break;
    case UI_EVT_BATTERY_FULL:
        UI_Popup_Show(Draw_SystemMsgPopup, "BATTERY FULL", 500);
        break;
    case UI_EVT_BATTERY_CHARGING:
        UI_Popup_Show(Draw_SystemMsgPopup, "CHARGING...", 500);
        break;
    case UI_EVT_POWEROUT:
        UI_Popup_Show(Draw_SystemMsgPopup, "POWER OUT", 500);
        break;
    case UI_EVT_POWERIN:
        UI_Popup_Show(Draw_SystemMsgPopup, "POWER IN", 500);
        break;
    case UI_EVT_WEATHER_TIME_SYNC:
        UI_Popup_Show(Draw_SystemMsgPopup, "SYNC WEATHER/TIME", 800);
        break;
    case UI_EVT_WEATHER_TIME_SYNC_OK:
        UI_Popup_Show(Draw_SystemMsgPopup, "TIME WEATHER OK", 800);
        break;
    case UI_EVT_KEY_TIMEOUT:
        UI_Popup_Show(Draw_SystemMsgPopup, "KEY TIMEOUT", 500);
        break;
    case UI_EVT_BULU_TIMEOUT:
        UI_Popup_Show(Draw_SystemMsgPopup, "BULU TIMEOUT", 500);
        break;
    case UI_EVT_MUSIC_TIMEOUT:
        UI_Popup_Show(Draw_SystemMsgPopup, "MUSIC TIMEOUT", 500);
        break;
    case UI_EVT_SHOW_ON:
        RTC_ReadToBuffer();
        Buffer_Swap();
        OLED_WakeUp();
        g_ui.oled_showing = 1;
        UI_Nav_WakeUp();
        break;
    case UI_EVT_SHOW_OFF:
        OLED_Sleep();
        g_ui.oled_showing = 0; 
        break;
    case UI_EVT_SLEEP_REQUEST:
        break;
      case UI_EVT_SYS_RUNNING:
        g_ui.sys_running = 1;
        break;      
        case UI_EVT_SYS_STOP:
        g_ui.sys_running = 0;
        break;        
    case UI_EVT_NONE:
    default:
        break;
    }
}
