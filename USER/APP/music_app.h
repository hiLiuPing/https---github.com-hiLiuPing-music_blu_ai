#ifndef __MUSIC_APP_H__
#define __MUSIC_APP_H__

/*
 * 音乐控制抽象层。
 * 对上提供统一命令，对下通过 GPIO 模拟按键和电源控制蓝牙音箱。
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 音乐控制命令。 */
typedef enum
{
    CMD_PLAY_STOP,  /* 播放/暂停 */
    CMD_PREV,       /* 上一曲 */
    CMD_NEXT,       /* 下一曲 */
    CMD_PAIR,       /* 配对 */
    CMD_CLEAR_PAIR, /* 清除配对记录 */
    CMD_POWER_ON,   /* 开机 */
    CMD_POWER_OFF,  /* 关机 */
    CMD_VOL_UP,     /* 音量+ */
    CMD_VOL_DOWN,    /* 音量- */
    CMD_SYSTEM_POWER_OFF, /* 系统关机 */
} MusicCtrlCmd;

/* 模拟按键所需的 GPIO 描述。 */
typedef struct
{
    GPIO_TypeDef* port;
    uint16_t      pin;
} KeyGpioTypeDef;

/* 统一按键编号，和板级 GPIO 映射保持一致。 */
typedef enum
{
    KEY1,
    KEY2,
    KEY3,
    // KEY4,
    // KEY5,
    // KEY6,
    KEY_NUM
} KeyIndexTypeDef;

// typedef struct
// {
//     uint8_t ble_connected;   // 当前是否连接
//     uint8_t ble_ever_connected; // 是否曾经连接过
// } BleState_t;

// extern BleState_t g_ble_state;

// typedef struct
// {
//     uint8_t music_played;   // 当前是否播放
//     uint8_t music_ever_played; // 是否曾经播放过
// } MusicState_t;

typedef struct
{
     uint8_t music_ble_power;   // 当前电源
    uint8_t ble_connected;   // 当前是否连接
    uint8_t ble_ever_connected; // 是否曾经连接过

    uint8_t music_played;   // 当前是否播放
    uint8_t music_ever_played; // 是否曾经播放过
} Music_Ble_State_t;

extern Music_Ble_State_t g_music_ble_state;

/* 命令投递接口。 */
void music_send_cmd(MusicCtrlCmd cmd);

/* 具体控制动作。 */
void Music_Play_Stop(void);
void Music_Up(void);
void Music_Next(void);
void Music_Pair(void);
void Music_ClearPair(void);
void Music_PowerOn(void);
void Music_PowerOff(void);
void Music_VolumeUp(void);
void Music_VolumeDown(void);
void System_PowerOff(void);

/* 按键模拟接口。 */
void SimKey_Click(KeyIndexTypeDef key);
void SimKey_DoubleClick(KeyIndexTypeDef key);
void SimKey_LongPress(KeyIndexTypeDef key, uint32_t sec);

#endif
