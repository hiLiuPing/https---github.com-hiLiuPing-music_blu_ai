/* app_sensors.h */

#ifndef __SENSORS_APP_H
#define __SENSORS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "i2c_bus.h"
#include "lis3dh_reg.h"

typedef struct
{
    I2C_Bus_t *bus;
    uint16_t dev_addr;
} generic_i2c_t;

typedef struct
{
    int16_t x[2];
    int16_t y[2];
    int16_t z[2];
    uint8_t buf_idx;
    lis3dh_ctx_t ctx;
} motion_module_t;

extern motion_module_t g_sensors_motion;
extern I2C_Bus_t i2c_bus_1;
extern I2C_Bus_t i2c_bus_2;

int32_t APP_Sensors_Init(void);
int32_t APP_Sensors_Update(void);
void Motion_SwapBuffer(motion_module_t *m);
void Update_Motion(motion_module_t *m);

#ifdef __cplusplus
}
#endif

#endif
