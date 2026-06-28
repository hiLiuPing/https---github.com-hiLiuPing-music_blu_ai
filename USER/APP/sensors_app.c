#include "sensors_app.h"

#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "eeprom_app.h"

/*
 * 传感器应用层。
 * 当前只封装运动模块的初始化和双缓冲刷新，方便上层直接读稳定数据。
 */

static int32_t generic_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;
    return I2C_Mem_Write(dev->bus, dev->dev_addr, reg, (uint8_t *)data, len);
}

static int32_t generic_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;
    return I2C_Mem_Read(dev->bus, dev->dev_addr, reg, data, len);
}

I2C_Bus_t i2c_bus_1 = {.hi2c = &hi2c1};
I2C_Bus_t i2c_bus_2 = {.hi2c = &hi2c2};

static generic_i2c_t dev_lis3dh = {
    .bus = &i2c_bus_2,
    .dev_addr = LIS3DH_ADDR,
};

motion_module_t g_sensors_motion = {0};

static void lis3dh_manual_init(lis3dh_ctx_t *ctx)
{
    uint8_t data;

    // 软件复位。
    data = 0x80;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG5, &data, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 100Hz 正常模式，开启 XYZ。
    data = 0x67;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG1, &data, 1);

    // BDU=1，量程 ±2g。
    data = 0x80;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG4, &data, 1);

    // 开高通滤波。
    data = 0x01;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG2, &data, 1);

    // 运动中断配置。
    data = 0x7F;
    ctx->write_reg(ctx->handle, LIS3DH_INT1_CFG, &data, 1);

    data = 8;
    ctx->write_reg(ctx->handle, LIS3DH_INT1_THS, &data, 1);

    data = 1;
    ctx->write_reg(ctx->handle, LIS3DH_INT1_DUR, &data, 1);

    data = 0x40;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG3, &data, 1);
}

static void Init_Motion(motion_module_t *m)
{
    m->ctx.handle = &dev_lis3dh;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    lis3dh_manual_init(&m->ctx);
}

/**
 * @brief  读取一次运动数据并写入后端缓冲
 */
void Update_Motion(motion_module_t *m)
{
    int16_t raw[3];

    if (lis3dh_acceleration_raw_get(&m->ctx, raw) == 0)
    {
        uint8_t back_buf = 1 - m->buf_idx;
        m->x[back_buf] = raw[0];
        m->y[back_buf] = raw[1];
        m->z[back_buf] = raw[2];
    }
}

/**
 * @brief  切换运动数据双缓冲
 */
void Motion_SwapBuffer(motion_module_t *m)
{
    m->buf_idx = 1 - m->buf_idx;
}

/**
 * @brief  传感器应用层初始化入口
 */
int32_t APP_Sensors_Init(void)
{
    Init_Motion(&g_sensors_motion);
    AppConfig_Init(&g_ee_ctx, &hi2c1, EE24_ADDRESS_DEFAULT);
    return 0;
}

/**
 * @brief  传感器应用层周期更新入口
 */
int32_t APP_Sensors_Update(void)
{
    Update_Motion(&g_sensors_motion);
    return 0;
}
