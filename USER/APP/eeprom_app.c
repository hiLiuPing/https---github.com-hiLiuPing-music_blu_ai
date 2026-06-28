#include "eeprom_app.h"
#include "log.h"

#include <string.h>

/*
 * EEPROM 配置层实现（单分区版）。
 * 通过 Magic + CRC 实现掉电校验，去掉 A/B 双区冗余。
 */

/* EEPROM context 实例。 */
eeprom_ctx_t g_ee_ctx;

/**
 * @brief  初始化 EEPROM
 * @param  ctx: context 指针
 * @param  hi2c: I2C 句柄
 * @param  dev_addr: 设备地址（8 位，如 0xA0）
 * @retval true 表示初始化成功
 */
bool AppConfig_Init(eeprom_ctx_t *ctx, I2C_HandleTypeDef *hi2c, uint8_t dev_addr)
{
    if (EE24_Init(&ctx->handle, hi2c, dev_addr))
    {
        ctx->initialized = 1;
        log_printf("EEPROM: Init OK (addr 0x%02X)\r\n", dev_addr);
        return true;
    }
    log_printf("EEPROM: Init FAILED!\r\n");
    return false;
}

/**
 * @brief  从 EEPROM 读取并校验
 * @param  offset: 物理偏移
 * @param  data: 输出缓存
 * @param  size: 结构体大小
 * @retval true 表示数据有效
 */
bool AppConfig_Load(uint32_t offset, void *data, uint16_t size)
{
    uint8_t *pBuf = (uint8_t*)data;

    if (!g_ee_ctx.initialized)
    {
        log_printf("EEPROM: not initialized!\r\n");
        return false;
    }

    if (!EE24_Read(&g_ee_ctx.handle, offset, pBuf, size, 1000))
    {
        log_printf("EEPROM: Read fail @ 0x%04lX\r\n", offset);
        return false;
    }

    uint32_t read_magic;
    memcpy(&read_magic, pBuf, 4);
    if (read_magic != 0x55AA55AA)
    {
        log_printf("EEPROM: Magic mismatch @ 0x%04lX\r\n", offset);
        return false;
    }

    uint16_t read_crc;
    memcpy(&read_crc, pBuf + size - 2, 2);

    uint16_t cal_crc = AppConfig_CRC16(pBuf, size - 2);
    if (cal_crc != read_crc)
    {
        log_printf("EEPROM: CRC fail @ 0x%04lX\r\n", offset);
        return false;
    }

    return true;
}

/**
 * @brief  写入 EEPROM（自动添加 Magic + CRC）
 * @param  offset: 物理偏移
 * @param  data: 输入缓存（前 4 字节供 Magic，末 2 字节供 CRC）
 * @param  size: 结构体大小
 * @retval true 表示写入成功
 */
bool AppConfig_Save(uint32_t offset, void *data, uint16_t size)
{
    uint8_t *pBuf = (uint8_t*)data;

    if (!g_ee_ctx.initialized)
    {
        log_printf("EEPROM: not initialized!\r\n");
        return false;
    }

    uint32_t magic = 0x55AA55AA;
    memcpy(pBuf, &magic, 4);

    uint16_t cal_crc = AppConfig_CRC16(pBuf, size - 2);
    memcpy(pBuf + size - 2, &cal_crc, 2);

    if (!EE24_Write(&g_ee_ctx.handle, offset, pBuf, size, 1000))
    {
        log_printf("EEPROM: Write fail @ 0x%04lX\r\n", offset);
        return false;
    }

    return true;
}

/**
 * @brief  计算 CRC16 (MODBUS)
 */
uint16_t AppConfig_CRC16(uint8_t *ptr, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--)
    {
        crc ^= *ptr++;
        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}
