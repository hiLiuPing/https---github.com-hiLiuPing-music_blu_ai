#ifndef __DATA_APP_H__
#define __DATA_APP_H__

/*
 * 数据中枢头文件。
 * 负责时间双缓冲、姿态判定、语录弹窗准备等 UI/业务共享数据的对外接口。
 */

#include "FreeRTOS.h"
#include "main.h"
#include "oled_popup.h"
#include "queue.h"
#include "sensors_app.h"
#include "task.h"
/* 姿态判定阈值。 */
#define RAD2DEG         57.29578f
#define ANGLE_TH        25.0f
#define ANGLE_HYST      20.0f
#define HOLD_COUNT      5

#define FALL_THRESHOLD  0.2f
#define FALL_TRIG_CNT   5

/* 语录渲染与滚动参数。 */
#define DATA_APP_QUOTE_MAX_LINES              256U
#define DATA_APP_QUOTE_RENDER_MAX_GLYPHS      96U
#define DATA_APP_QUOTE_FONT_W                 16U
#define DATA_APP_QUOTE_FONT_H                 16U
#define DATA_APP_QUOTE_RENDER_MAX_W           (DATA_APP_QUOTE_RENDER_MAX_GLYPHS * DATA_APP_QUOTE_FONT_W)
#define DATA_APP_QUOTE_RENDER_STRIDE          (DATA_APP_QUOTE_RENDER_MAX_W / 8U)
#define DATA_APP_QUOTE_RENDER_BUF_SIZE        (DATA_APP_QUOTE_RENDER_STRIDE * DATA_APP_QUOTE_FONT_H)
#define DATA_APP_QUOTE_VIEW_W                 116U
#define DATA_APP_QUOTE_SCROLL_STEP_MS         15U
// #define DATA_APP_QUOTE_DEFAULT_PERIOD_MS      180000U  /* 3 分钟 */

#define DATA_APP_QUOTE_DEFAULT_PERIOD_MS      50000U  /* 60s */




#define BUF_NUM 2

#define DATA_APP_QUOTE_FILE_PATH         "/quotes_china.txt"
#define DATA_APP_QUOTE_FONT_PATH         "/heiti_1_16.bin"
#define DATA_APP_QUOTE_FONT_BYTES        32U
#define DATA_APP_QUOTE_ASCII_GLYPHS      96U
#define DATA_APP_QUOTE_CJK_START         0x4E00UL
#define DATA_APP_QUOTE_CJK_END           0x9FA5UL
#define DATA_APP_QUOTE_CJK_GLYPHS        ((DATA_APP_QUOTE_CJK_END - DATA_APP_QUOTE_CJK_START) + 1U)
#define DATA_APP_QUOTE_FULL_START        0xFF01UL
#define DATA_APP_QUOTE_FULL_END          0xFF60UL
#define DATA_APP_QUOTE_FULL_GLYPHS       ((DATA_APP_QUOTE_FULL_END - DATA_APP_QUOTE_FULL_START) + 1U)
#define DATA_APP_QUOTE_LATIN1_START      0x00B0UL
#define DATA_APP_QUOTE_LATIN1_END        0x00BEUL
#define DATA_APP_QUOTE_LATIN1_GLYPHS     ((DATA_APP_QUOTE_LATIN1_END - DATA_APP_QUOTE_LATIN1_START) + 1U)
#define DATA_APP_QUOTE_CJK_PUNC_START    0x3000UL
#define DATA_APP_QUOTE_CJK_PUNC_END      0x303FUL
#define DATA_APP_QUOTE_CJK_PUNC_GLYPHS   ((DATA_APP_QUOTE_CJK_PUNC_END - DATA_APP_QUOTE_CJK_PUNC_START) + 1U)
#define DATA_APP_QUOTE_FONT_GLYPHS       (DATA_APP_QUOTE_ASCII_GLYPHS + DATA_APP_QUOTE_CJK_GLYPHS + DATA_APP_QUOTE_FULL_GLYPHS + DATA_APP_QUOTE_LATIN1_GLYPHS + DATA_APP_QUOTE_CJK_PUNC_GLYPHS)
#define DATA_APP_QUOTE_FONT_SIZE         (DATA_APP_QUOTE_FONT_GLYPHS * DATA_APP_QUOTE_FONT_BYTES)
#define DATA_APP_QUOTE_SCROLL_HOLD_MS    2000U
#define DATA_APP_QUOTE_SCROLL_EDGE_PAUSE_MS 1000U
#define DATA_APP_QUOTE_MIN_DURATION_MS   4000U
#define DATA_APP_QUOTE_MAX_DURATION_MS   6000U


/* RTC 时间快照。 */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} time_t;

/* 时间双缓冲与格式化。 */
void Time_Init(void);
void Time_Get(time_t *out);
uint8_t Time_GetColon(void);
void Time_BlinkUpdate(void);
void RTC_ReadToBuffer(void);
void Buffer_Swap(void);
void Time_Format(char *out);



/* 原始输入事件。 */
typedef enum {
    MSG_TILT_NONE = 0,
    MSG_TILT_UP,
    MSG_TILT_DOWN,
    MSG_TILT_LEFT,
    MSG_TILT_RIGHT,
    MSG_BLUETOOTH_CONNECT,
    MSG_BLUETOOTH_DISCONNECT,
    MSG_MUSIC_PLAY,
    MSG_MUSIC_STOP,
    MSG_PWER_IN,
    MSG_PWER_OUT,
    MSG_PWER_CHAGNE,
    MSG_PWER_FULL,
    MSG_FALL_DOWN,
    MSG_TILT_SHAKE_VERTICAL,
    MSG_TILT_SHAKE_HORIZONTAL,
} TiltKey_t;

/* LittleFS 语录索引。 */
typedef struct {
    uint32_t offset;
    uint32_t length;
} DataApp_QuoteLine_t;

/* 待显示语录请求。 */
typedef struct {
    char text[POPUP_MSG_BUF_LEN];
    uint32_t duration_ms;
} DataApp_QuotePopupRequest_t;

/* 已预渲染语录帧。 */
typedef struct {
    char text[POPUP_MSG_BUF_LEN];
    uint8_t bitmap[DATA_APP_QUOTE_RENDER_BUF_SIZE];
    uint16_t bitmap_width;
    uint16_t visible_offset_x;
    uint16_t visible_width;
    uint16_t scroll_px;
    uint16_t scroll_limit;
    uint32_t scroll_hold_ms;
    uint32_t scroll_edge_pause_ms;
    uint32_t duration_ms;
    uint8_t valid;
} DataApp_QuotePopupFrame_t;

/* 当前原始方向，供调试或其它模块只读。 */
extern TiltKey_t current_raw_direction;

/* 数据中枢接口。 */
void DataApp_Init(void);
void DataApp_QuoteInvalidate(void);
void DataApp_QuoteServiceUpdate(TickType_t now);
uint8_t DataApp_QuoteShowNext(DataApp_QuotePopupRequest_t *out);
uint8_t DataApp_QuotePopup_CopyFrame(DataApp_QuotePopupFrame_t *out);
/* 以下旧接口保留以兼容外部引用，新代码请使用 DataApp_QuoteShowNext。 */
uint8_t DataApp_QuotePopup_PeekPending(DataApp_QuotePopupRequest_t *out);
void DataApp_QuotePopup_CommitPending(TickType_t now);

/* 运动判定入口。 */
TiltKey_t TiltKey_Update(motion_module_t *motion);
TiltKey_t FallDetect_Check(motion_module_t *motion);

#endif
