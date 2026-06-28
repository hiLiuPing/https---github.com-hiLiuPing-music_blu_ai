#ifndef __EEPROM_APP_H__
#define __EEPROM_APP_H__

/*
 * EEPROM 配置层（单分区版）。
 * 使用 context 结构体，统一在 sensors_app.h 初始化。
 * 单块(256B)偏移管理：NetConfig(0)、CalibData(256)、Weather(512)。
 */

#include "main.h"
#include "ee24.h"

/* EEPROM context，类似 lis3dh_ctx_t 模式。 */
typedef struct {
    EE24_HandleTypeDef handle;
    uint8_t initialized;
} eeprom_ctx_t;

extern eeprom_ctx_t g_ee_ctx;

#define EE_BLOCK_SIZE       (256)

/* 单分区内各配置块的偏移。 */
#define OFF_NET_CONFIG      (0)
#define OFF_CALIB_DATA      (EE_BLOCK_SIZE * 1)

/* 网络配置。 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    char     ssid[32];
    char     pwd[64];
    char     weather_url[128];
    uint16_t crc;
} NetConfig_t;

/* 传感器标定配置。 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    float    sensor_offset;
    int      calibration_val;
    uint16_t crc;
} CalibData_t;

#define STATIC_ASSERT(COND, MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
STATIC_ASSERT(sizeof(NetConfig_t) <= EE_BLOCK_SIZE, NetConfig_Too_Large);
STATIC_ASSERT(sizeof(CalibData_t) <= EE_BLOCK_SIZE, CalibData_Too_Large);

/* ── API ── */

/* EEPROM 初始化（由 sensors_app 调用） */
bool AppConfig_Init(eeprom_ctx_t *ctx, I2C_HandleTypeDef *hi2c, uint8_t dev_addr);

uint16_t AppConfig_CRC16(uint8_t *ptr, uint16_t len);

/* 单分区读写（自动 magic + CRC 校验） */
bool AppConfig_Save(uint32_t offset, void *data, uint16_t size);
bool AppConfig_Load(uint32_t offset, void *data, uint16_t size);

#endif /* __EEPROM_APP_H__ */
