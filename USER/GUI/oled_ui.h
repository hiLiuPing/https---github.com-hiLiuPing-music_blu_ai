#ifndef __OLED_UI_H__
#define __OLED_UI_H__

#include "FreeRTOS.h"
#include "i2c_bus.h"
#include "main.h"
#include "oled_anim.h"
#include "oled_popup.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAGE_BOOT = 0,
    PAGE_HOME,
    PAGE_CLOCK,
    PAGE_PAGE1,
    PAGE_PAGE2,
    PAGE_MAX,
} UI_Page_t;

typedef enum {
    UI_REGION_IDLE = 0,
    UI_REGION_SLIDING
} UI_RegionState_t;

typedef enum {
    UI_STAGE_NORMAL = 0,
    UI_STAGE_EXITING,
    UI_STAGE_ENTERING
} UI_Stage_t;

typedef enum {
    UI_SLIDE_DOWN = 0,
    UI_SLIDE_UP
} UI_SlideDir_t;

typedef enum {
    UI_EXT_STATIC = 0,
    UI_EXT_DYNAMIC
} UI_ExtPageMode_t;

typedef void (*UI_DrawFunc_t)(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t w, uint8_t h);

typedef enum {
    UI_EVT_NONE = 0,
    UI_EVT_PREV,
    UI_EVT_NEXT,
    UI_EVT_PLAY,
    UI_EVT_PAUSE,
    UI_EVT_STOP,
    UI_EVT_VOL_UP,
    UI_EVT_VOL_DOWN,
    UI_EVT_TEMPERATURE_HIGH,
    UI_EVT_TEMPERATURE_NORMAL,
    UI_EVT_BLUETOOTH_CONNECTED,
    UI_EVT_BLUETOOTH_DISCONNECTED,
    UI_EVT_BATTERY_LOW,
    UI_EVT_BATTERY_FULL,
    UI_EVT_BATTERY_CHARGING,
    UI_EVT_POWEROUT,
    UI_EVT_POWERIN,
    UI_EVT_SHOW_ON,
    UI_EVT_SHOW_OFF,
    UI_EVT_SLEEP_REQUEST,
    UI_EVT_SYS_RUNNING,
    UI_EVT_SYS_STOP,
    UI_EVT_WEATHER_TIME_SYNC,
    UI_EVT_WEATHER_TIME_SYNC_OK,
    UI_EVT_KEY_TIMEOUT,
    UI_EVT_BULU_TIMEOUT,
    UI_EVT_MUSIC_ON,
    UI_EVT_MUSIC_OFF,
    UI_EVT_MUSIC_TIMEOUT,
} UI_Event_t;

typedef struct {
    UI_Page_t cur_page;
    UI_Page_t next_page;
    UI_Stage_t stage;
    struct {
        uint8_t batterypower;
        uint8_t percent;
        uint8_t is_charging;
    } battery;
    int8_t temp;
    uint8_t oled_showing;
    uint8_t boot_completed;
    uint8_t sys_running;
} UI_Global_t;

extern volatile UI_Global_t g_ui;

void OLED_UI_Init(I2C_Bus_t *bus);
void OLED_UI_Update(void);
void OLED_UI_Render(void);
void OLED_UI_SetPage(UI_Page_t page);
void OLED_UI_OnEvent(UI_Event_t event);
void OLED_UI_SetBattery(uint8_t percent, uint8_t charging);

#ifdef __cplusplus
}
#endif

#endif
