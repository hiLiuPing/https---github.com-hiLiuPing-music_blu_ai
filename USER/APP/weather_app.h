#ifndef __WEATHER_APP_H__
#define __WEATHER_APP_H__

/*
 * 串口天气/协议解析层 + ESP32 电源控制 + EEPROM 持久化。
 * 负责接收外部模块帧数据，并把时间、天气和空气质量拆解成业务数据。
 */

#include "main.h"
#include "uart_dma.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define FRAME_HEAD      0xAA
#define FRAME_TAIL      0x55

typedef enum {
    CMD_GET_TIME        = 0x01,
    CMD_GET_NOW_DETAIL  = 0x02,
    CMD_GET_AIR_DETAIL  = 0x03,
    CMD_GET_FUTURE_7DAY = 0x04,
    CMD_RESTART         = 0x05,
    CMD_OTA_START       = 0x11,
    CMD_FS_LIST         = 0x20,
    CMD_FS_SELECT       = 0x22,
    CMD_FS_SAVE         = 0x30
} Weather_cmd_t;

typedef struct {
    Weather_cmd_t type;
    char filename[64];
} Weather_msg_t;

typedef struct
{
    char date[16];
    char weather[16];
    int temp_high;
    int temp_low;
    int icon_id;
} WeatherDay_t;

typedef enum
{
    STATE_IDLE,
    STATE_CMD,
    STATE_LEN_H,
    STATE_LEN_L,
    STATE_DATA,
    STATE_CRC,
    STATE_TAIL
} ProtocolState_t;

typedef struct {
    char text[32];
    int  icon;
    int  temp;
    int  feelsLike;
    char windDir[32];
    int  windScale;
    int  vis;
    int  humidity;
    int  aqi;
    int  pm10;
    int  pm25;
    int  no2;
    int  so2;
    float co;
    int  o3;
} WeatherData_t;

typedef struct
{
    int aqi;
    int pm10;
    int pm25;
    int no2;
    int so2;
    float co;
    int o3;
} AirQuality_t;

typedef struct {
    uint8_t first_sync_done;
    uint8_t power_on;
    uint8_t syncing;
} WeatherModule_t;

extern WeatherData_t g_now_weather;
extern AirQuality_t g_air_detail;
extern WeatherDay_t g_future_weather[7];
extern uart_dma_t uart1_admin;
extern uint8_t u1_dma_buf[512];
extern uint8_t u1_rb_buf[1024];
extern RTC_HandleTypeDef hrtc;

extern WeatherModule_t g_weather_module;
extern volatile uint8_t g_weather_persist_dirty;

void process_protocol_data(uint8_t cmd, char *data);
uint8_t stm32_calc_crc8(uint8_t *ptr, uint16_t len);

void Weather_PowerOn(void);
void Weather_PowerOff(void);
void Weather_SendCommand(Weather_cmd_t cmd);
void Weather_FillDemoData(void);
void Weather_BeginSyncCycle(void);
uint8_t Weather_HasCompletedSync(void);
void Weather_DebugPrintAll(void);

#endif /* __WEATHER_APP_H__ */
