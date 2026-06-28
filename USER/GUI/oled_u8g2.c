#include "oled_u8g2.h"

#include <string.h>

#include "FreeRTOS.h"
#include "log.h"
#include "task.h"
#include "u8g2.h"

/*
 * OLED U8g2 adapter.
 * Keep u8g2's original transaction granularity and only replace the physical
 * I2C transmit with a DMA-backed path for runtime refresh.
 */

static u8g2_t u8g2;
static I2C_Bus_t *oled_bus;
static uint8_t oled_i2c_addr = OLED_I2C_ADDR;
static uint8_t oled_i2c_error_logged = 0U;
static uint8_t oled_dma_runtime_enabled = 0U;

#define OLED_I2C_TX_BUF_SIZE   128U
#define OLED_DMA_INVALID_IDX   0xFFU
#define OLED_DMA_IDLE_TIMEOUT_MS     20U
#define OLED_DMA_IDLE_WARN_MS         4U

typedef struct
{
    uint8_t tx_buf[2][OLED_I2C_TX_BUF_SIZE];
    uint16_t tx_len[2];
    uint8_t build_index;
    uint8_t dma_index;
    uint8_t pending_index;
    uint8_t dma_busy;
} OLED_DmaTx_t;

typedef struct
{
    uint32_t dma_busy_set_count;
    uint32_t dma_busy_clear_count;
    uint32_t tx_complete_count;
    uint32_t error_callback_count;
    uint32_t tx_fail_count;
    uint32_t wait_idle_warn_count;
    uint32_t wait_idle_timeout_count;
    uint32_t recover_count;
    uint32_t wake_count;
    uint32_t sleep_count;
} OLED_DebugStats_t;

static OLED_DmaTx_t g_oled_dma_tx;
static OLED_DebugStats_t g_oled_debug_stats;

static I2C_HandleTypeDef *OLED_GetI2CHandle(void);
static uint8_t OLED_ProbeAddress(I2C_Bus_t *bus);
static void OLED_DmaReset(void);
static uint8_t OLED_DmaStartBuffer(uint8_t idx);
static uint8_t OLED_DmaTryKick(void);
static uint8_t OLED_DmaWaitIdle(TickType_t timeout_ticks);
static uint8_t OLED_DmaWaitPendingConsumed(uint8_t idx, TickType_t timeout_ticks);
static uint8_t OLED_TransmitBlocking(const uint8_t *buf, uint16_t len);
static void OLED_DmaStopRuntime(void);
static void OLED_DmaResumeRuntime(void);
static void OLED_DmaRecoverRuntime(const char *reason);

static I2C_HandleTypeDef *OLED_GetI2CHandle(void)
{
    return I2C_Bus_GetHandle(oled_bus);
}

static uint8_t OLED_ProbeAddress(I2C_Bus_t *bus)
{
    static const uint8_t candidates[] = {
        0x3C,
        0x3D,
    };
    uint8_t i;

    for (i = 0; i < (uint8_t)(sizeof(candidates) / sizeof(candidates[0])); i++)
    {
        if (I2C_IsDeviceReady(bus, candidates[i], 2, 50) == HAL_OK)
        {
            return candidates[i];
        }
    }

    return OLED_I2C_ADDR;
}

static void OLED_DmaReset(void)
{
    memset(&g_oled_dma_tx, 0, sizeof(g_oled_dma_tx));
    g_oled_dma_tx.dma_index = OLED_DMA_INVALID_IDX;
    g_oled_dma_tx.pending_index = OLED_DMA_INVALID_IDX;
}

static uint8_t OLED_DmaStartBuffer(uint8_t idx)
{
    I2C_HandleTypeDef *hi2c = OLED_GetI2CHandle();

    if (idx >= 2U || g_oled_dma_tx.tx_len[idx] == 0U)
    {
        return 0U;
    }

    if (I2C_Write_DMA_Start(oled_bus,
                            oled_i2c_addr,
                            g_oled_dma_tx.tx_buf[idx],
                            g_oled_dma_tx.tx_len[idx]) != HAL_OK)
    {
        if (!oled_i2c_error_logged)
        {
            log_printf("[OLED] I2C DMA tx fail addr=0x%02X err=0x%08lX len=%u\r\n",
                       oled_i2c_addr,
                       (unsigned long)((hi2c != NULL) ? hi2c->ErrorCode : 0UL),
                       g_oled_dma_tx.tx_len[idx]);
            oled_i2c_error_logged = 1U;
        }
        return 0U;
    }

    oled_i2c_error_logged = 0U;
    g_oled_dma_tx.dma_busy = 1U;
    g_oled_dma_tx.dma_index = idx;
    g_oled_debug_stats.dma_busy_set_count++;
    return 1U;
}

static uint8_t OLED_DmaTryKick(void)
{
    if (g_oled_dma_tx.dma_busy)
    {
        return 1U;
    }

    if (g_oled_dma_tx.pending_index != OLED_DMA_INVALID_IDX)
    {
        uint8_t idx = g_oled_dma_tx.pending_index;
        g_oled_dma_tx.pending_index = OLED_DMA_INVALID_IDX;
        return OLED_DmaStartBuffer(idx);
    }

    return 0U;
}

static uint8_t OLED_DmaWaitIdle(TickType_t timeout_ticks)
{
    I2C_HandleTypeDef *hi2c = OLED_GetI2CHandle();
    TickType_t start_tick;
    uint8_t warned = 0U;

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return 1U;
    }

    start_tick = xTaskGetTickCount();
    while (g_oled_dma_tx.dma_busy)
    {
        TickType_t elapsed = xTaskGetTickCount() - start_tick;

        if (!warned && elapsed >= pdMS_TO_TICKS(OLED_DMA_IDLE_WARN_MS))
        {
            warned = 1U;
            g_oled_debug_stats.wait_idle_warn_count++;
            log_printf("[OLED] wait idle busy tick=%lu\r\n", (unsigned long)elapsed);
        }

        if (elapsed >= timeout_ticks)
        {
            g_oled_debug_stats.wait_idle_timeout_count++;
            log_printf("[OLED] wait idle timeout err=0x%08lX state=%lu\r\n",
                       (unsigned long)((hi2c != NULL) ? hi2c->ErrorCode : 0UL),
                       (unsigned long)((hi2c != NULL) ? HAL_I2C_GetState(hi2c) : 0UL));
            return 0U;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return 1U;
}

static uint8_t OLED_DmaWaitPendingConsumed(uint8_t idx, TickType_t timeout_ticks)
{
    TickType_t start_tick;

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return 0U;
    }

    start_tick = xTaskGetTickCount();
    while (g_oled_dma_tx.pending_index == idx)
    {
        if (!g_oled_dma_tx.dma_busy)
        {
            (void)OLED_DmaTryKick();
        }

        if ((TickType_t)(xTaskGetTickCount() - start_tick) >= timeout_ticks)
        {
            g_oled_debug_stats.tx_fail_count++;
            log_printf("[OLED] pending timeout idx=%u busy=%u\r\n",
                       idx,
                       g_oled_dma_tx.dma_busy);
            OLED_DmaRecoverRuntime("pending");
            return 0U;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return 1U;
}

static uint8_t OLED_TransmitBlocking(const uint8_t *buf, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = OLED_GetI2CHandle();

    if (len == 0U)
    {
        return 1U;
    }

    if (I2C_Write(oled_bus, oled_i2c_addr, buf, len) != HAL_OK)
    {
        g_oled_debug_stats.tx_fail_count++;
        if (!oled_i2c_error_logged)
        {
            log_printf("[OLED] I2C tx fail addr=0x%02X err=0x%08lX len=%u\r\n",
                       oled_i2c_addr,
                       (unsigned long)((hi2c != NULL) ? hi2c->ErrorCode : 0UL),
                       len);
            oled_i2c_error_logged = 1U;
        }
        return 0U;
    }

    oled_i2c_error_logged = 0U;
    return 1U;
}

static void OLED_DmaStopRuntime(void)
{
    oled_dma_runtime_enabled = 0U;
    OLED_DmaReset();
}

static void OLED_DmaResumeRuntime(void)
{
    OLED_DmaReset();
    oled_dma_runtime_enabled = 1U;
}

static void OLED_DmaRecoverRuntime(const char *reason)
{
    I2C_HandleTypeDef *hi2c = OLED_GetI2CHandle();
    HAL_StatusTypeDef status;

    g_oled_debug_stats.recover_count++;

    if (hi2c == NULL)
    {
        OLED_DmaReset();
        return;
    }

    if (hi2c->hdmatx != NULL)
    {
        (void)HAL_DMA_Abort(hi2c->hdmatx);
    }

    if (I2C_Bus_IsLocked(oled_bus))
    {
        I2C_Bus_Unlock(oled_bus);
    }

    status = HAL_I2C_DeInit(hi2c);
    if (status != HAL_OK)
    {
        log_printf("[OLED] recover deinit fail status=%d reason=%s\r\n",
                   status,
                   (reason != NULL) ? reason : "?");
    }

    status = HAL_I2C_Init(hi2c);
    if (status != HAL_OK)
    {
        log_printf("[OLED] recover init fail status=%d reason=%s\r\n",
                   status,
                   (reason != NULL) ? reason : "?");
    }

    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    NVIC_ClearPendingIRQ(I2C2_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C2_ER_IRQn);

    oled_i2c_error_logged = 0U;
    OLED_DmaReset();

    log_printf("[OLED] recover #%lu reason=%s err=0x%08lX\r\n",
               (unsigned long)g_oled_debug_stats.recover_count,
               (reason != NULL) ? reason : "?",
               (unsigned long)hi2c->ErrorCode);
}

uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t idx = g_oled_dma_tx.build_index;

    (void)u8x8;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        break;

    case U8X8_MSG_BYTE_SET_DC:
        /*
         * SSD13xx fast I2C CAD already injects 0x00/0x40 control bytes via
         * U8X8_MSG_BYTE_SEND. The byte layer must forward raw bytes unchanged.
         */
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        idx = g_oled_dma_tx.build_index;
        g_oled_dma_tx.tx_len[idx] = 0U;
        break;

    case U8X8_MSG_BYTE_SEND:
    {
        uint8_t *data = (uint8_t *)arg_ptr;

        idx = g_oled_dma_tx.build_index;
        if ((uint16_t)(g_oled_dma_tx.tx_len[idx] + arg_int) > OLED_I2C_TX_BUF_SIZE)
        {
            if (!oled_i2c_error_logged)
            {
                log_printf("[OLED] I2C tx overflow len=%u add=%u buf=%u\r\n",
                           g_oled_dma_tx.tx_len[idx],
                           arg_int,
                           OLED_I2C_TX_BUF_SIZE);
                oled_i2c_error_logged = 1U;
            }
            return 0;
        }

        memcpy(&g_oled_dma_tx.tx_buf[idx][g_oled_dma_tx.tx_len[idx]], data, arg_int);
        g_oled_dma_tx.tx_len[idx] = (uint16_t)(g_oled_dma_tx.tx_len[idx] + arg_int);
        oled_i2c_error_logged = 0U;
        break;
    }

    case U8X8_MSG_BYTE_END_TRANSFER:
    {
        uint8_t finished_idx;

        idx = g_oled_dma_tx.build_index;
        finished_idx = idx;

        if ((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) || !oled_dma_runtime_enabled)
        {
            if (!OLED_TransmitBlocking(g_oled_dma_tx.tx_buf[idx], g_oled_dma_tx.tx_len[idx]))
            {
                return 0;
            }
            break;
        }

        if (g_oled_dma_tx.tx_len[finished_idx] == 0U)
        {
            break;
        }

        if (!g_oled_dma_tx.dma_busy)
        {
            if (!OLED_DmaStartBuffer(finished_idx))
            {
                return 0;
            }

            g_oled_dma_tx.build_index = (uint8_t)(finished_idx ^ 1U);
            g_oled_dma_tx.tx_len[g_oled_dma_tx.build_index] = 0U;
            break;
        }

        if (g_oled_dma_tx.pending_index != OLED_DMA_INVALID_IDX)
        {
            if (!OLED_DmaWaitPendingConsumed(g_oled_dma_tx.pending_index, pdMS_TO_TICKS(20)))
            {
                return 0;
            }
        }

        g_oled_dma_tx.pending_index = finished_idx;
        if (!OLED_DmaWaitPendingConsumed(finished_idx, pdMS_TO_TICKS(20)))
        {
            return 0;
        }

        g_oled_dma_tx.build_index = (uint8_t)(g_oled_dma_tx.dma_busy ? (g_oled_dma_tx.dma_index ^ 1U) : (finished_idx ^ 1U));
        g_oled_dma_tx.tx_len[g_oled_dma_tx.build_index] = 0U;
        break;
    }

    default:
        return 0;
    }

    return 1;
}

uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg)
    {
    case U8X8_MSG_DELAY_MILLI:
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        {
            vTaskDelay(pdMS_TO_TICKS(arg_int));
        }
        else
        {
            HAL_Delay(arg_int);
        }
        break;

    case U8X8_MSG_DELAY_10MICRO:
        for (volatile uint32_t i = 0; i < (SystemCoreClock / 1000000U); i++)
        {
        }
        break;

    default:
        return 1;
    }

    return 1;
}

void OLED_Init(I2C_Bus_t *bus)
{
    oled_bus = bus;
    oled_i2c_addr = OLED_ProbeAddress(bus);
    OLED_DmaReset();
    oled_dma_runtime_enabled = 0U;
    memset(&g_oled_debug_stats, 0, sizeof(g_oled_debug_stats));

    if (oled_i2c_addr != OLED_I2C_ADDR)
    {
        log_printf("[OLED] detected addr 0x%02X\r\n", oled_i2c_addr);
    }
    else if (I2C_IsDeviceReady(bus, oled_i2c_addr, 2, 50) != HAL_OK)
    {
        log_printf("[OLED] no ACK on 0x%02X, init continues with default\r\n", oled_i2c_addr);
    }

    u8g2_Setup_ssd1312_i2c_128x32_f(&u8g2, U8G2_R0, u8x8_byte_hw_i2c, u8x8_gpio_and_delay);
    u8g2_SetI2CAddress(&u8g2, (uint8_t)(oled_i2c_addr << 1));
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    OLED_DmaReset();
    oled_dma_runtime_enabled = 1U;
}

void OLED_Clear(void)
{
    u8g2_ClearBuffer(&u8g2);
}

void OLED_Update(void)
{
    u8g2_SendBuffer(&u8g2);
}

void OLED_DrawStr(uint8_t x, uint8_t y, const char *str)
{
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, x, y, str);
}

u8g2_t *OLED_GetHandle(void)
{
    return &u8g2;
}

void OLED_Sleep(void)
{
    g_oled_debug_stats.sleep_count++;
    if (!OLED_DmaWaitIdle(pdMS_TO_TICKS(OLED_DMA_IDLE_TIMEOUT_MS)))
    {
        OLED_DmaRecoverRuntime("sleep");
    }
    OLED_DmaStopRuntime();
    u8g2_SetPowerSave(&u8g2, 1);
    log_printf("[OLED] sleep #%lu\r\n", (unsigned long)g_oled_debug_stats.sleep_count);
}

void OLED_WakeUp(void)
{
    g_oled_debug_stats.wake_count++;
    OLED_DmaRecoverRuntime("wake");
    OLED_DmaResumeRuntime();
    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    NVIC_ClearPendingIRQ(I2C2_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C2_ER_IRQn);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SendBuffer(&u8g2);
    log_printf("[OLED] wake #%lu\r\n", (unsigned long)g_oled_debug_stats.wake_count);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != OLED_GetI2CHandle())
    {
        return;
    }

    g_oled_debug_stats.tx_complete_count++;
    g_oled_debug_stats.dma_busy_clear_count++;
    g_oled_dma_tx.dma_busy = 0U;
    g_oled_dma_tx.dma_index = OLED_DMA_INVALID_IDX;
    I2C_Bus_UnlockFromISR(oled_bus);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != OLED_GetI2CHandle())
    {
        return;
    }

    g_oled_debug_stats.error_callback_count++;
    g_oled_debug_stats.dma_busy_clear_count++;
    g_oled_dma_tx.dma_busy = 0U;
    g_oled_dma_tx.dma_index = OLED_DMA_INVALID_IDX;
    I2C_Bus_UnlockFromISR(oled_bus);

    if (!oled_i2c_error_logged)
    {
        log_printf("[OLED] I2C error callback err=0x%08lX\r\n",
                   (unsigned long)hi2c->ErrorCode);
        oled_i2c_error_logged = 1U;
    }
}
