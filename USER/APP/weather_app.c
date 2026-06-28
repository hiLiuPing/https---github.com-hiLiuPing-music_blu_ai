#include "weather_app.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "usart.h"

AirQuality_t g_air_detail;
WeatherData_t g_now_weather;
WeatherDay_t g_future_weather[7];
uart_dma_t uart1_admin;
uint8_t u1_dma_buf[512];
uint8_t u1_rb_buf[1024];

WeatherModule_t g_weather_module = {0};
volatile uint8_t g_weather_persist_dirty = 0;

static uint8_t g_weather_cycle_has_now = 0U;
static uint8_t g_weather_cycle_has_air = 0U;
static uint8_t g_weather_cycle_has_future = 0U;

static int extract_int(char *str)
{
    char *p;

    if (str == NULL)
    {
        return 0;
    }

    p = str;
    while (*p && (*p < '0' || *p > '9') && *p != '-')
    {
        p++;
    }

    if (*p == '\0')
    {
        return 0;
    }

    return atoi(p);
}

void Weather_BeginSyncCycle(void)
{
    g_weather_cycle_has_now = 0U;
    g_weather_cycle_has_air = 0U;
    g_weather_cycle_has_future = 0U;
}

uint8_t Weather_HasCompletedSync(void)
{
    return (uint8_t)((g_weather_cycle_has_now != 0U) &&
                     (g_weather_cycle_has_air != 0U) &&
                     (g_weather_cycle_has_future != 0U));
}

void Weather_FillDemoData(void)
{
    strcpy(g_now_weather.text, "Rain");
    g_now_weather.icon = 100;
    g_now_weather.temp = 22;
    g_now_weather.feelsLike = 24;
    strcpy(g_now_weather.windDir, "East");
    g_now_weather.windScale = 0;
    g_now_weather.vis = 2;
    g_now_weather.humidity = 15;
    g_now_weather.aqi = 95;
    g_now_weather.pm10 = 22;
    g_now_weather.pm25 = 13;
    g_now_weather.no2 = 10;
    g_now_weather.so2 = 6;
    g_now_weather.co = 5.00f;
    g_now_weather.o3 = 0;

    g_air_detail.aqi = 22;
    g_air_detail.pm10 = 13;
    g_air_detail.pm25 = 10;
    g_air_detail.no2 = 6;
    g_air_detail.so2 = 5;
    g_air_detail.co = 0.00f;
    g_air_detail.o3 = 70;

    strcpy(g_future_weather[0].date, "2026-06-13");
    strcpy(g_future_weather[0].weather, "HeavyRain");
    g_future_weather[0].temp_high = 23;
    g_future_weather[0].temp_low = 20;
    g_future_weather[0].icon_id = 307;

    strcpy(g_future_weather[1].date, "2026-06-14");
    strcpy(g_future_weather[1].weather, "Cloudy");
    g_future_weather[1].temp_high = 32;
    g_future_weather[1].temp_low = 21;
    g_future_weather[1].icon_id = 101;

    strcpy(g_future_weather[2].date, "2026-06-15");
    strcpy(g_future_weather[2].weather, "Cloudy");
    g_future_weather[2].temp_high = 33;
    g_future_weather[2].temp_low = 23;
    g_future_weather[2].icon_id = 101;

    strcpy(g_future_weather[3].date, "2026-06-16");
    strcpy(g_future_weather[3].weather, "Rain");
    g_future_weather[3].temp_high = 34;
    g_future_weather[3].temp_low = 24;
    g_future_weather[3].icon_id = 305;

    strcpy(g_future_weather[4].date, "2026-06-17");
    strcpy(g_future_weather[4].weather, "Rain");
    g_future_weather[4].temp_high = 34;
    g_future_weather[4].temp_low = 25;
    g_future_weather[4].icon_id = 305;

    strcpy(g_future_weather[5].date, "2026-06-18");
    strcpy(g_future_weather[5].weather, "MidRain");
    g_future_weather[5].temp_high = 30;
    g_future_weather[5].temp_low = 25;
    g_future_weather[5].icon_id = 306;

    strcpy(g_future_weather[6].date, "2026-06-19");
    strcpy(g_future_weather[6].weather, "MidRain");
    g_future_weather[6].temp_high = 29;
    g_future_weather[6].temp_low = 25;
    g_future_weather[6].icon_id = 306;
}

uint8_t stm32_calc_crc8(uint8_t *ptr, uint16_t len)
{
    uint8_t crc = 0x00;
    while (len--)
    {
        crc ^= *ptr++;
        for (uint8_t i = 8; i > 0; --i)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

void RTC_SyncFromString(const char *time_str)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    int year, month, day;
    int hour, min, sec;

    if (sscanf(time_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6)
    {
        return;
    }

    sDate.Year = year - 2000;
    sDate.Month = month;
    sDate.Date = day;
    sTime.Hours = hour;
    sTime.Minutes = min;
    sTime.Seconds = sec;

    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
}

RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

void process_protocol_data(uint8_t cmd, char *data)
{
    char *ptr;

    switch (cmd)
    {
    case CMD_GET_TIME:
        RTC_SyncFromString(data);
        log_printf("[Time] Received: %s\r\n", data);
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        log_printf("RTC: 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
                   sDate.Year, sDate.Month, sDate.Date,
                   sTime.Hours, sTime.Minutes, sTime.Seconds);
        break;

    case CMD_GET_NOW_DETAIL:
        ptr = strtok(data, ",");
        if (ptr) strncpy(g_now_weather.text, ptr, sizeof(g_now_weather.text) - 1U);

        ptr = strtok(NULL, ",");
        if (ptr) g_now_weather.icon = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_now_weather.temp = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_now_weather.feelsLike = extract_int(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) strncpy(g_now_weather.windDir, ptr, sizeof(g_now_weather.windDir) - 1U);

        ptr = strtok(NULL, ",");
        if (ptr) g_now_weather.vis = extract_int(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_now_weather.humidity = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_now_weather.aqi = atoi(ptr);

        ptr = strtok(NULL, ","); if (ptr) g_now_weather.pm10 = atoi(ptr);
        ptr = strtok(NULL, ","); if (ptr) g_now_weather.pm25 = atoi(ptr);
        ptr = strtok(NULL, ","); if (ptr) g_now_weather.no2 = atoi(ptr);
        ptr = strtok(NULL, ","); if (ptr) g_now_weather.so2 = atoi(ptr);
        ptr = strtok(NULL, ","); if (ptr) g_now_weather.co = atof(ptr);
        ptr = strtok(NULL, ","); if (ptr) g_now_weather.o3 = atoi(ptr);

        g_weather_cycle_has_now = 1U;
        log_printf("[Weather] Now: %s, Icon: %d, Temp: %d, Feels: %d, Vis: %d\r\n",
                   g_now_weather.text, g_now_weather.icon, g_now_weather.temp,
                   g_now_weather.feelsLike, g_now_weather.vis);
        break;

    case CMD_GET_AIR_DETAIL:
        ptr = strtok(data, ",");
        if (ptr) g_air_detail.aqi = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_air_detail.pm10 = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_air_detail.pm25 = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_air_detail.no2 = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_air_detail.so2 = atoi(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_air_detail.co = atof(ptr);

        ptr = strtok(NULL, ",");
        if (ptr) g_air_detail.o3 = atoi(ptr);

        g_weather_cycle_has_air = 1U;
        log_printf("[Air] AQI: %d, PM2.5: %d\r\n", g_air_detail.aqi, g_air_detail.pm25);
        break;

    case CMD_GET_FUTURE_7DAY:
        if (strcmp(data, "LIST_START") == 0)
        {
            log_printf("[Future] Sync Start...\r\n");
        }
        else if (strcmp(data, "LIST_END") == 0)
        {
            g_weather_cycle_has_future = 1U;
            log_printf("[Future] Sync Complete!\r\n");
        }
        else
        {
            char *future_ptr;
            int idx = -1;

            future_ptr = strtok(data, "|");
            if (future_ptr)
            {
                idx = atoi(future_ptr);
            }

            if (idx >= 0 && idx < 7)
            {
                future_ptr = strtok(NULL, "|");
                if (future_ptr) strncpy(g_future_weather[idx].date, future_ptr, sizeof(g_future_weather[idx].date) - 1U);

                future_ptr = strtok(NULL, "|");
                if (future_ptr) strncpy(g_future_weather[idx].weather, future_ptr, sizeof(g_future_weather[idx].weather) - 1U);

                future_ptr = strtok(NULL, "|");
                if (future_ptr) g_future_weather[idx].temp_high = atoi(future_ptr);

                future_ptr = strtok(NULL, "|");
                if (future_ptr) g_future_weather[idx].temp_low = atoi(future_ptr);

                future_ptr = strtok(NULL, "|");
                if (future_ptr) g_future_weather[idx].icon_id = atoi(future_ptr);
            }
        }
        break;

    default:
        break;
    }
}

void Weather_PowerOn(void)
{
    if (!g_weather_module.power_on)
    {
        HAL_GPIO_WritePin(ESP32_PWR_EN_GPIO_Port, ESP32_PWR_EN_Pin, GPIO_PIN_SET);
        g_weather_module.power_on = 1U;
        log_printf("[Weather] ESP32 ON\r\n");
    }
}

void Weather_PowerOff(void)
{
    if (g_weather_module.power_on)
    {
        HAL_GPIO_WritePin(ESP32_PWR_EN_GPIO_Port, ESP32_PWR_EN_Pin, GPIO_PIN_RESET);
        g_weather_module.power_on = 0U;
        log_printf("[Weather] ESP32 OFF\r\n");
    }
}

void Weather_SendCommand(Weather_cmd_t cmd)
{
    HAL_UART_Transmit(&huart1, (uint8_t[]){FRAME_HEAD, cmd, FRAME_TAIL}, 3, 100);
    log_printf("[Weather] TX CMD 0x%02X\r\n", cmd);
}

void Weather_DebugPrintAll(void)
{
    uint8_t i;

    log_printf("\r\n========== NOW WEATHER ==========\r\n");
    log_printf("Text      : %s\r\n", g_now_weather.text);
    log_printf("Icon      : %d\r\n", g_now_weather.icon);
    log_printf("Temp      : %d C\r\n", g_now_weather.temp);
    log_printf("FeelsLike : %d C\r\n", g_now_weather.feelsLike);
    log_printf("WindDir   : %s\r\n", g_now_weather.windDir);
    log_printf("WindScale : %d\r\n", g_now_weather.windScale);
    log_printf("Visibility: %d km\r\n", g_now_weather.vis);
    log_printf("Humidity  : %d %%\r\n", g_now_weather.humidity);
    log_printf("AQI       : %d\r\n", g_now_weather.aqi);
    log_printf("PM10      : %d\r\n", g_now_weather.pm10);
    log_printf("PM2.5     : %d\r\n", g_now_weather.pm25);
    log_printf("NO2       : %d\r\n", g_now_weather.no2);
    log_printf("SO2       : %d\r\n", g_now_weather.so2);
    log_printf("CO        : %.2f\r\n", g_now_weather.co);
    log_printf("O3        : %d\r\n", g_now_weather.o3);

    log_printf("\r\n========== AIR QUALITY ==========\r\n");
    log_printf("AQI       : %d\r\n", g_air_detail.aqi);
    log_printf("PM10      : %d\r\n", g_air_detail.pm10);
    log_printf("PM2.5     : %d\r\n", g_air_detail.pm25);
    log_printf("NO2       : %d\r\n", g_air_detail.no2);
    log_printf("SO2       : %d\r\n", g_air_detail.so2);
    log_printf("CO        : %.2f\r\n", g_air_detail.co);
    log_printf("O3        : %d\r\n", g_air_detail.o3);

    log_printf("\r\n========== FUTURE 7 DAYS ==========\r\n");
    for (i = 0; i < 7; i++)
    {
        log_printf("[%u]\r\n"
                   "Date      : %s\r\n"
                   "Weather   : %s\r\n"
                   "High Temp : %d C\r\n"
                   "Low Temp  : %d C\r\n"
                   "Icon ID   : %d\r\n\r\n",
                   i,
                   g_future_weather[i].date,
                   g_future_weather[i].weather,
                   g_future_weather[i].temp_high,
                   g_future_weather[i].temp_low,
                   g_future_weather[i].icon_id);
    }

    log_printf("===================================\r\n");
}
