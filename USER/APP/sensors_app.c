#include "sensors_app.h"

#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "eeprom_app.h"
#include "log.h"
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
/* =========================================================
 * SHT40 专用（COMMAND MODE）
 * ========================================================= */
static int32_t sht40_write_cmd(generic_i2c_t *dev, uint8_t cmd)
{
    return I2C_Write(dev->bus,
                     dev->dev_addr,
                     &cmd,
                     1);
}

static int32_t sht40_read_data(generic_i2c_t *dev,
                                uint8_t *buf,
                                uint16_t len)
{
    return I2C_Read(dev->bus,
                    dev->dev_addr,
                    buf,
                    len);
}
I2C_Bus_t i2c_bus_1 = {.hi2c = &hi2c1};
I2C_Bus_t i2c_bus_2 = {.hi2c = &hi2c2};

static generic_i2c_t dev_lis3dh = {
    .bus = &i2c_bus_2,
    .dev_addr = LIS3DH_ADDR,
};

static generic_i2c_t dev_max17048 = {
    .bus = &i2c_bus_2,
    .dev_addr = MAX17048_ADDR,
};

static generic_i2c_t dev_sht40 = {
    .bus = &i2c_bus_2,
    .dev_addr = SHT40_ADDR,
};

motion_module_t g_sensors_motion = {0};
battery_module_t g_sensors_battery;
env_module_t g_sensors_environment;


/* =========================================================
 * BATTERY INIT
 * ========================================================= */
static void Init_Battery(battery_module_t *m)
{
    m->ctx.handle = &dev_max17048;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg  = generic_i2c_read;

    max17048_init(&m->ctx);
}



/* =========================================================
 * ENV INIT (SHT40)
 * ========================================================= */
static void Init_Env(env_module_t *m)
{
    m->ctx.handle = &dev_sht40;
    m->ctx.write_reg = NULL;
    m->ctx.read_reg  = NULL;
}


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
/* =========================================================
 * ENV UPDATE (SHT40)
 * ========================================================= */
void Update_Env(env_module_t *m)
{
    uint8_t cmd = SHT40_MEAS_HIGH_PRECISION;
    uint8_t rx[6];

    generic_i2c_t *dev = (generic_i2c_t *)m->ctx.handle;

    if (sht40_write_cmd(dev, cmd) != 0)
        return;

    HAL_Delay(10);

    if (sht40_read_data(dev, rx, 6) != 0)
        return;

    if (SHT40_CRC8(&rx[0], 2) != rx[2]) return;
    if (SHT40_CRC8(&rx[3], 2) != rx[5]) return;

    uint16_t t = (rx[0] << 8) | rx[1];
    uint16_t h = (rx[3] << 8) | rx[4];

    m->temp = sht40_convert_temp(t);
    m->hum  = sht40_convert_hum(h);
}

/* =========================================================
 * BATTERY UPDATE
 * ========================================================= */
void Update_Battery(battery_module_t *m)
{
    float soc, voltage;

    if (max17048_get_soc(&m->ctx, &soc) == 0)
    {
        if (soc < 0.0f)
            soc = 0.0f;
        else if (soc > 100.0f)
            soc = 100.0f;
        m->soc = soc;
    }

    if (max17048_get_vcell(&m->ctx, &voltage) == 0)
        m->voltage = voltage / 1000.0f;
//   log_printf("voltage: %f, soc: %f", m->voltage, m->soc);
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
    Init_Battery(&g_sensors_battery);
    Init_Env(&g_sensors_environment);
    return 0;
}

/**
 * @brief  传感器应用层周期更新入口
 */
int32_t APP_Sensors_Update(void)
{
    Update_Motion(&g_sensors_motion);
        Update_Env(&g_sensors_environment);
            Update_Battery(&g_sensors_battery);
    return 0;
}
