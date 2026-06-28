/* Private includes -----------------------------------------------------------*/
// includes
#include "tim.h"
#include "i2c.h"
#include "usart.h"
#include "user_TasksInit.h"

#include "key.h"
#include "log.h"
#include "data_app.h"
#include "music_app.h"
#include "oled_ui.h"
#include "rgb_led.h"
#include "sensors_app.h"
#include "weather_app.h"
#include "psram_app.h"
#include "lfs_port.h"
#include "systemMonitor_app.h"
#include "lptim.h"

spi_flash_t flash_32mb = {0};

LED_Object_t led_blue;  // PE3 -> TIM3_CH1
LED_Object_t led_green; // PE4 -> TIM3_CH2
LED_Object_t led_red;   // PE5 -> TIM3_CH3

RGB_Object_t rgb;

extern lfs_t g_lfs;

/* 绉佹湁鍑芥暟 ------------------------------------------------------------*/
void list_all_files(void)
{
    lfs_dir_t dir;
    struct lfs_info info;
    int ret;

    lfs_port_lock();
    ret = lfs_dir_open(&g_lfs, &dir, "/");
    if (ret < 0)
    {
        lfs_port_unlock();
        log_printf("Open root dir failed: %d\r\n", ret);
        return;
    }

    log_printf("\r\n====== File List ======\r\n");

    while (1)
    {
        ret = lfs_dir_read(&g_lfs, &dir, &info);
        if (ret < 0)
        {
            log_printf("Read dir failed: %d\r\n", ret);
            break;
        }

        if (ret == 0)
        {
            break;
        }

        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0)
        {
            continue;
        }

        if (info.type == LFS_TYPE_REG)
        {
            log_printf("[FILE] %-32s %6lu KB\r\n", info.name, (uint32_t)((info.size + 1023) / 1024));
        }
        else if (info.type == LFS_TYPE_DIR)
        {
            log_printf("[DIR ] %s\r\n", info.name);
        }
    }

    lfs_dir_close(&g_lfs, &dir);
    lfs_port_unlock();
    log_printf("=======================\r\n");
}

/**
 * @brief  纭欢鍒濆鍖栦换鍔?
 * @note   杩欐槸绯荤粺涓婄數鍚庣殑鍏ュ彛浠诲姟锛岄『搴忛€氬父鏄棩蹇?-> LED -> 涓插彛 -> 鎸夐敭
 *         -> Flash/LittleFS -> 鏁版嵁灞?-> UI -> 浼犳劅鍣?鐩戞帶 -> 澶栬渚涚數銆?
 * @param  argument: 鏈娇鐢?
 * @retval None
 */
void HardwareInitTask(void *argument)
{
    (void)argument;

    g_ui.sys_running = 1;
    g_ui.battery.is_charging =
        (HAL_GPIO_ReadPin(BAT_CHG_GPIO_Port, BAT_CHG_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    g_ui.battery.batterypower =
        (HAL_GPIO_ReadPin(POWER_IN_5V_GPIO_Port, POWER_IN_5V_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    g_ui.battery.percent = 0U;

    log_init(&huart2);
    log_printf("log init done.\n");

    LED_Driver_Init(&led_blue, LED_B_GPIO_Port, LED_B_Pin, &htim2, TIM_CHANNEL_1, 1);
    LED_Driver_Init(&led_red, LED_R_GPIO_Port, LED_R_Pin, &htim2, TIM_CHANNEL_3, 1);
    LED_Driver_Init(&led_green, LED_G_GPIO_Port, LED_G_Pin, &htim2, TIM_CHANNEL_2, 1);

    RGB_Init(&rgb, &led_red, &led_green, &led_blue);
    RGB_SendCmd(&rgb, RGB_EFFECT_RAINBOW, 5000, 0, 0, 0);
    log_printf("led init done.\n");

    uart_dma_init(&uart1_admin, &huart1, u1_dma_buf, UART_Transmit_DMA_RX_SIZE, u1_rb_buf, UART_Transmit_LWRB_SIZE);
    log_printf("uart dma init done.\n");

    Key_Init();
    log_printf("key init done.\n");
    I2C_Bus_Init(&i2c_bus_1);
    I2C_Bus_Init(&i2c_bus_2);
    OLED_UI_Init(&i2c_bus_1);
    log_printf("oled init done.\n");
    if (spi_flash_init(&flash_32mb, &hspi1, SPI1_CS_GPIO_Port, SPI1_CS_Pin) != 0)
    {
        log_printf("Flash Hardware Init Failed!\r\n");
        return;
    }

    lfs_port_init(&flash_32mb);
    log_printf("flash init done.\n");
    DataApp_Init();
    log_printf("data app init done.\n");
    Weather_PowerOn();
    // 开启esp32
    Time_Init();
    APP_Sensors_Init();
    UserMonitor_Init();
    LPTIM_Start1Hz();
    LPTIM_SetQuoteInterval(180);
    LPTIM_StartIO1(180U);
    LPTIM_StartIO2(300U);
    g_weather_module.first_sync_done = 0U;
    log_printf("[Weather] boot sync required\r\n");
    
    if (PSRAM_App_Init() != 0)
    {
        log_printf("PSRAM App Init Failed!\r\n");
    }

    Music_PowerOff();
    g_music_ble_state.music_ble_power = 0;
    vTaskDelete(NULL);
}
