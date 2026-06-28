#include "data_app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "lfs_port.h"
#include "log.h"

#if FLASH_USE_FREERTOS
#include "semphr.h"
#endif

/*
 * DataApp 是 UI 与业务之间的数据中枢，主要承担三类工作：
 * 1. 时间数据双缓冲，供时钟 widget 只读获取；
 * 2. 倾斜/跌倒等姿态结果的业务封装；
 * 3. 基于 LittleFS 的中文语录索引、预渲染和弹窗调度。
 */

extern RTC_HandleTypeDef hrtc;

// #define BUF_NUM 2

// #define DATA_APP_QUOTE_FILE_PATH         "/quotes_china.txt"
// #define DATA_APP_QUOTE_FONT_PATH         "/heiti_1_16.bin"
// #define DATA_APP_QUOTE_FONT_BYTES        32U
// #define DATA_APP_QUOTE_ASCII_GLYPHS      96U
// #define DATA_APP_QUOTE_CJK_START         0x4E00UL
// #define DATA_APP_QUOTE_CJK_END           0x9FA5UL
// #define DATA_APP_QUOTE_CJK_GLYPHS        ((DATA_APP_QUOTE_CJK_END - DATA_APP_QUOTE_CJK_START) + 1U)
// #define DATA_APP_QUOTE_FULL_START        0xFF01UL
// #define DATA_APP_QUOTE_FULL_END          0xFF60UL
// #define DATA_APP_QUOTE_FULL_GLYPHS       ((DATA_APP_QUOTE_FULL_END - DATA_APP_QUOTE_FULL_START) + 1U)
// #define DATA_APP_QUOTE_LATIN1_START      0x00B0UL
// #define DATA_APP_QUOTE_LATIN1_END        0x00BEUL
// #define DATA_APP_QUOTE_LATIN1_GLYPHS     ((DATA_APP_QUOTE_LATIN1_END - DATA_APP_QUOTE_LATIN1_START) + 1U)
// #define DATA_APP_QUOTE_CJK_PUNC_START    0x3000UL
// #define DATA_APP_QUOTE_CJK_PUNC_END      0x303FUL
// #define DATA_APP_QUOTE_CJK_PUNC_GLYPHS   ((DATA_APP_QUOTE_CJK_PUNC_END - DATA_APP_QUOTE_CJK_PUNC_START) + 1U)
// #define DATA_APP_QUOTE_FONT_GLYPHS       (DATA_APP_QUOTE_ASCII_GLYPHS + DATA_APP_QUOTE_CJK_GLYPHS + DATA_APP_QUOTE_FULL_GLYPHS + DATA_APP_QUOTE_LATIN1_GLYPHS + DATA_APP_QUOTE_CJK_PUNC_GLYPHS)
// #define DATA_APP_QUOTE_FONT_SIZE         (DATA_APP_QUOTE_FONT_GLYPHS * DATA_APP_QUOTE_FONT_BYTES)
// #define DATA_APP_QUOTE_SCROLL_HOLD_MS    2000U
// #define DATA_APP_QUOTE_SCROLL_EDGE_PAUSE_MS 1000U
// #define DATA_APP_QUOTE_MIN_DURATION_MS   5000U
// #define DATA_APP_QUOTE_MAX_DURATION_MS   8000U

typedef enum {
    DATA_APP_QUOTE_SCROLL_PHASE_HOLD = 0,
    DATA_APP_QUOTE_SCROLL_PHASE_LEFT,
    DATA_APP_QUOTE_SCROLL_PHASE_LEFT_PAUSE,
    DATA_APP_QUOTE_SCROLL_PHASE_RIGHT,
    DATA_APP_QUOTE_SCROLL_PHASE_RIGHT_PAUSE,
} DataApp_QuoteScrollPhase_t;

typedef struct {
    char text[POPUP_MSG_BUF_LEN];
    uint8_t bitmap[DATA_APP_QUOTE_RENDER_BUF_SIZE];
    uint16_t bitmap_width;
    uint16_t visible_offset_x;
    uint16_t visible_width;
    uint16_t scroll_limit;
    uint32_t scroll_hold_ms;
    uint32_t scroll_edge_pause_ms;
    uint32_t duration_ms;
    uint16_t scroll_px;
    uint8_t valid;
} DataApp_QuotePrepared_t;

typedef struct {
    DataApp_QuotePrepared_t active;
    DataApp_QuotePrepared_t pending;
    TickType_t active_start_tick;
    TickType_t last_refresh_check_tick;
    uint32_t file_size;
    uint16_t line_count;
    uint16_t last_pick;
    DataApp_QuoteScrollPhase_t active_scroll_phase;
    uint8_t initialized;
    uint8_t cache_loaded;
    uint8_t font_checked;
    uint8_t active_started_showing;
} DataApp_QuoteRuntime_t;

static void DataApp_QuoteResetRuntime(void);
static int DataApp_QuoteOpenReadonlyFile(lfs_file_t *file, struct lfs_file_config *cfg, const char *path);
static void DataApp_QuoteTrimLineBounds(char *buf, uint16_t *start, uint16_t *end);
static void DataApp_QuoteBuildIndex(void);
static uint8_t DataApp_QuoteReadIndexedLine(uint16_t line_index, char *out, size_t out_len);
static void DataApp_QuoteLoadCache(void);
static void DataApp_QuoteValidateFontFileOnce(void);
/* UTF-8 与字模映射工具。 */
static uint32_t DataApp_QuoteDecodeUtf8(const char **text);
static uint32_t DataApp_QuoteNormalizeCodepoint(uint32_t codepoint);
static int32_t DataApp_QuoteGlyphIndex(uint32_t codepoint);
static uint8_t DataApp_QuoteSelectRandomLineLocked(uint16_t *picked_index);
static uint32_t DataApp_QuoteDurationForWidth(uint16_t width_px);
/**
 * @brief  统计预渲染位图的可见区域
 * @retval None
 */
static void DataApp_QuoteMeasureBitmap(const uint8_t *bitmap,
                                       uint16_t width_px,
                                       uint16_t *visible_offset_x,
                                       uint16_t *visible_width);
static void DataApp_QuoteClearBitmap(uint8_t *bitmap);
static void DataApp_QuoteSetPixel(uint8_t *bitmap, uint16_t x, uint8_t y);
static void DataApp_QuoteRenderGlyph(uint8_t *bitmap, uint16_t x_offset, const uint8_t glyph[DATA_APP_QUOTE_FONT_BYTES]);
/**
 * @brief  读取单个字模
 * @retval 0 表示成功
 */
static int DataApp_QuoteReadGlyph(lfs_t *lfs,
                                  lfs_file_t *file,
                                  uint32_t codepoint,
                                  uint8_t glyph[DATA_APP_QUOTE_FONT_BYTES]);
/**
 * @brief  根据行号构建语录预渲染帧
 * @retval 1 表示成功
 */
static uint8_t DataApp_QuoteBuildPrepared(uint16_t line_index, DataApp_QuotePrepared_t *out);
/**
 * @brief  推进当前 active 语录的滚动状态
 * @retval None
 */
static void DataApp_QuoteUpdateScrollLocked(TickType_t now);

TiltKey_t current_raw_direction = MSG_TILT_NONE;

static time_t time_buf[BUF_NUM];
static volatile uint8_t time_write_idx = 0;
static volatile uint8_t time_read_idx  = 1;
static DataApp_QuoteRuntime_t s_quote_runtime;
static DataApp_QuoteLine_t s_quote_lines[DATA_APP_QUOTE_MAX_LINES];
static uint8_t s_quote_file_buf[LFS_CACHE_SIZE];
static DataApp_QuotePrepared_t s_quote_prepare_scratch;

#if FLASH_USE_FREERTOS
static SemaphoreHandle_t s_data_app_lock = NULL;
static void DataApp_Lock(void)
{
    if (s_data_app_lock != NULL)
    {
        (void)xSemaphoreTake(s_data_app_lock, portMAX_DELAY);
    }
}

static void DataApp_Unlock(void)
{
    if (s_data_app_lock != NULL)
    {
        (void)xSemaphoreGive(s_data_app_lock);
    }
}
#else
static void DataApp_Lock(void) {}
static void DataApp_Unlock(void) {}
#endif

static uint8_t colon = 1;

/**
 * @brief  翻转时钟冒号的闪烁状态
 * @retval None
 */
void Time_BlinkUpdate(void)
{
    colon = !colon;
}

/**
 * @brief  从 RTC 读取当前时间并写入写缓冲
 * @retval None
 */
void RTC_ReadToBuffer(void)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    time_buf[time_write_idx].year  = 2000 + sDate.Year;
    time_buf[time_write_idx].month = sDate.Month;
    time_buf[time_write_idx].date  = sDate.Date;
    time_buf[time_write_idx].hour  = sTime.Hours;
    time_buf[time_write_idx].min   = sTime.Minutes;
    time_buf[time_write_idx].sec   = sTime.Seconds;
}

/**
 * @brief  交换时间双缓冲读写索引
 * @retval None
 */
inline void Buffer_Swap(void)
{
    uint8_t tmp = time_write_idx;
    time_write_idx = time_read_idx;
    time_read_idx = tmp;
}

/**
 * @brief  读取当前时间快照
 * @param  out: 输出时间结构体
 * @retval None
 */
void Time_Get(time_t *out)
{
    if (out == NULL)
    {
        return;
    }

    *out = time_buf[time_read_idx];
}

/**
 * @brief  读取当前冒号闪烁状态
 * @retval 1 表示显示冒号，0 表示空格
 */
uint8_t Time_GetColon(void)
{
    return colon;
}

/**
 * @brief  初始化时间双缓冲
 * @retval None
 */
void Time_Init(void)
{
    time_buf[0] = (time_t){0};
    time_buf[1] = (time_t){0};
}

/**
 * @brief  把当前时间格式化为字符串
 * @param  out: 输出缓存，至少 6 字节
 * @retval None
 */
void Time_Format(char *out)
{
    time_t t;
    uint8_t col;

    if (out == NULL)
    {
        return;
    }

    Time_Get(&t);
    col = Time_GetColon();

    if (col)
    {
        sprintf(out, "%02d:%02d", t.hour, t.min);
    }
    else
    {
        sprintf(out, "%02d %02d", t.hour, t.min);
    }
}

/**
 * @brief  清空语录运行时状态
 * @retval None
 */
static void DataApp_QuoteResetRuntime(void)
{
    memset(&s_quote_runtime, 0, sizeof(s_quote_runtime));
    s_quote_runtime.last_pick = UINT16_MAX;
}

/**
 * @brief  打开只读 LittleFS 文件
 * @retval 0 以上表示成功，负值表示失败
 */
static int DataApp_QuoteOpenReadonlyFile(lfs_file_t *file, struct lfs_file_config *cfg, const char *path)
{
    int ret;
    lfs_t *lfs = lfs_port_get();

    ret = lfs_file_opencfg(lfs, file, path, LFS_O_RDONLY, cfg);
    if (ret < 0 && path != NULL && path[0] == '/')
    {
        ret = lfs_file_opencfg(lfs, file, path + 1, LFS_O_RDONLY, cfg);
    }

    return ret;
}

/**
 * @brief  去掉一行文本的首尾空白
 * @retval None
 */
static void DataApp_QuoteTrimLineBounds(char *buf, uint16_t *start, uint16_t *end)
{
    if (buf == NULL || start == NULL || end == NULL || *end < *start)
    {
        return;
    }

    while (*start < *end)
    {
        char ch = buf[*start];

        if (ch == ' ' || ch == '\t' || ch == '\r')
        {
            (*start)++;
        }
        else
        {
            break;
        }
    }

    while (*end > *start)
    {
        char ch = buf[*end - 1U];

        if (ch == ' ' || ch == '\t' || ch == '\r')
        {
            (*end)--;
        }
        else
        {
            break;
        }
    }
}

/**
 * @brief  为语录文件建立行索引
 * @retval None
 */
static void DataApp_QuoteBuildIndex(void)
{
    lfs_t *lfs = lfs_port_get();
    lfs_file_t file;
    struct lfs_file_config cfg;
    uint8_t read_buf[64];
    lfs_ssize_t read_ret;
    uint32_t file_pos = 0U;
    uint32_t line_start = 0U;
    uint16_t line_count = 0U;

    memset(s_quote_lines, 0, sizeof(s_quote_lines));
    s_quote_runtime.file_size = 0U;
    s_quote_runtime.line_count = 0U;
    s_quote_runtime.cache_loaded = 0U;

    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_quote_file_buf;

    lfs_port_lock();
    log_printf("[DataApp] quote cache open\r\n");
    if (DataApp_QuoteOpenReadonlyFile(&file, &cfg, DATA_APP_QUOTE_FILE_PATH) < 0)
    {
        lfs_port_unlock();
        log_printf("[DataApp] quote file open failed\r\n");
        return;
    }

    while ((read_ret = lfs_file_read(lfs, &file, read_buf, sizeof(read_buf))) > 0)
    {
        uint8_t i;

        for (i = 0U; i < (uint8_t)read_ret; i++)
        {
            if (read_buf[i] == '\n')
            {
                uint32_t raw_end = file_pos;

                if (raw_end > line_start && line_count < DATA_APP_QUOTE_MAX_LINES)
                {
                    s_quote_lines[line_count].offset = line_start;
                    s_quote_lines[line_count].length = (raw_end - line_start);
                    line_count++;
                }

                line_start = file_pos + 1U;
            }

            file_pos++;
        }
    }

    if (file_pos > line_start && line_count < DATA_APP_QUOTE_MAX_LINES)
    {
        s_quote_lines[line_count].offset = line_start;
        s_quote_lines[line_count].length = (file_pos - line_start);
        line_count++;
    }

    (void)lfs_file_close(lfs, &file);
    lfs_port_unlock();
    s_quote_runtime.file_size = file_pos;
    s_quote_runtime.line_count = line_count;
    s_quote_runtime.cache_loaded = 1U;
    s_quote_runtime.last_pick = UINT16_MAX;
    s_quote_runtime.last_refresh_check_tick = xTaskGetTickCount();

    log_printf("[DataApp] quote cache load done size=%lu lines=%u\r\n",
               (unsigned long)s_quote_runtime.file_size,
               (unsigned int)s_quote_runtime.line_count);
}

/**
 * @brief  从 LittleFS 读取语录文件到内存缓存
 * @retval None
 */
static void DataApp_QuoteLoadCache(void)
{
    DataApp_QuoteBuildIndex();
}

static uint32_t DataApp_QuoteDecodeUtf8(const char **text)
{
    const uint8_t *src = (const uint8_t *)(*text);
    uint32_t codepoint;

    if (src == NULL || src[0] == 0U)
    {
        return 0U;
    }

    if (src[0] < 0x80U)
    {
        (*text)++;
        return src[0];
    }

    if ((src[0] & 0xE0U) == 0xC0U && src[1] != 0U)
    {
        codepoint = (uint32_t)(src[0] & 0x1FU) << 6;
        codepoint |= (uint32_t)(src[1] & 0x3FU);
        *text += 2;
        return codepoint;
    }

    if ((src[0] & 0xF0U) == 0xE0U && src[1] != 0U && src[2] != 0U)
    {
        codepoint = (uint32_t)(src[0] & 0x0FU) << 12;
        codepoint |= (uint32_t)(src[1] & 0x3FU) << 6;
        codepoint |= (uint32_t)(src[2] & 0x3FU);
        *text += 3;
        return codepoint;
    }

    if ((src[0] & 0xF8U) == 0xF0U && src[1] != 0U && src[2] != 0U && src[3] != 0U)
    {
        codepoint = (uint32_t)(src[0] & 0x07U) << 18;
        codepoint |= (uint32_t)(src[1] & 0x3FU) << 12;
        codepoint |= (uint32_t)(src[2] & 0x3FU) << 6;
        codepoint |= (uint32_t)(src[3] & 0x3FU);
        *text += 4;
        return codepoint;
    }

    (*text)++;
    return '?';
}

static uint32_t DataApp_QuoteNormalizeCodepoint(uint32_t codepoint)
{
    switch (codepoint)
    {
    case 0x0009U:
    case 0x000AU:
    case 0x000DU:
        return 0x0020U;
    case 0x2018U:
    case 0x2019U:
        return 0x0027U;
    case 0x201CU:
    case 0x201DU:
        return 0x0022U;
    case 0x300AU:
        return 0xFF1CU;
    case 0x300BU:
        return 0xFF1EU;
    default:
        return codepoint;
    }
}

static int32_t DataApp_QuoteGlyphIndex(uint32_t codepoint)
{
    codepoint = DataApp_QuoteNormalizeCodepoint(codepoint);

    if (codepoint >= 0x20U && codepoint <= 0x7FU)
    {
        return (int32_t)(codepoint - 0x20U);
    }

    if (codepoint >= DATA_APP_QUOTE_CJK_START && codepoint <= DATA_APP_QUOTE_CJK_END)
    {
        return (int32_t)(DATA_APP_QUOTE_ASCII_GLYPHS + (codepoint - DATA_APP_QUOTE_CJK_START));
    }

    if (codepoint >= DATA_APP_QUOTE_FULL_START && codepoint <= DATA_APP_QUOTE_FULL_END)
    {
        return (int32_t)(DATA_APP_QUOTE_ASCII_GLYPHS + DATA_APP_QUOTE_CJK_GLYPHS + (codepoint - DATA_APP_QUOTE_FULL_START));
    }

    if (codepoint >= DATA_APP_QUOTE_LATIN1_START && codepoint <= DATA_APP_QUOTE_LATIN1_END)
    {
        return (int32_t)(DATA_APP_QUOTE_ASCII_GLYPHS + DATA_APP_QUOTE_CJK_GLYPHS + DATA_APP_QUOTE_FULL_GLYPHS + (codepoint - DATA_APP_QUOTE_LATIN1_START));
    }

    if (codepoint >= DATA_APP_QUOTE_CJK_PUNC_START && codepoint <= DATA_APP_QUOTE_CJK_PUNC_END)
    {
        return (int32_t)(DATA_APP_QUOTE_ASCII_GLYPHS +
                         DATA_APP_QUOTE_CJK_GLYPHS +
                         DATA_APP_QUOTE_FULL_GLYPHS +
                         DATA_APP_QUOTE_LATIN1_GLYPHS +
                         (codepoint - DATA_APP_QUOTE_CJK_PUNC_START));
    }

    return -1;
}

static uint8_t DataApp_QuoteReadIndexedLine(uint16_t line_index, char *out, size_t out_len)
{
    lfs_t *lfs = lfs_port_get();
    lfs_file_t file;
    struct lfs_file_config cfg;
    uint8_t read_buf[64];
    DataApp_QuoteLine_t line;
    uint32_t remaining;
    size_t write_pos = 0U;
    lfs_soff_t seek_ret;
    lfs_ssize_t read_ret;

    if (out == NULL || out_len == 0U || line_index >= s_quote_runtime.line_count)
    {
        return 0U;
    }

    line = s_quote_lines[line_index];
    if (line.length == 0U)
    {
        return 0U;
    }

    memset(out, 0, out_len);

    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_quote_file_buf;

    lfs_port_lock();
    if (DataApp_QuoteOpenReadonlyFile(&file, &cfg, DATA_APP_QUOTE_FILE_PATH) < 0)
    {
        lfs_port_unlock();
        log_printf("[DataApp] quote file reopen failed\r\n");
        return 0U;
    }

    seek_ret = lfs_file_seek(lfs, &file, (lfs_soff_t)line.offset, LFS_SEEK_SET);
    if (seek_ret < 0)
    {
        (void)lfs_file_close(lfs, &file);
        lfs_port_unlock();
        return 0U;
    }

    remaining = line.length;
    while (remaining > 0U && write_pos < (out_len - 1U))
    {
        lfs_size_t chunk = (remaining > sizeof(read_buf)) ? sizeof(read_buf) : (lfs_size_t)remaining;
        size_t copy_len;

        read_ret = lfs_file_read(lfs, &file, read_buf, chunk);
        if (read_ret <= 0)
        {
            break;
        }

        copy_len = (size_t)read_ret;
        if (copy_len > (out_len - 1U - write_pos))
        {
            copy_len = (out_len - 1U - write_pos);
        }

        memcpy(&out[write_pos], read_buf, copy_len);
        write_pos += copy_len;
        remaining -= (uint32_t)read_ret;

        if (copy_len < (size_t)read_ret)
        {
            break;
        }
    }

    (void)lfs_file_close(lfs, &file);
    lfs_port_unlock();
    out[write_pos] = '\0';

    if (line.offset == 0U &&
        write_pos >= 3U &&
        (uint8_t)out[0] == 0xEFU &&
        (uint8_t)out[1] == 0xBBU &&
        (uint8_t)out[2] == 0xBFU)
    {
        memmove(out, out + 3, write_pos - 3U);
        write_pos -= 3U;
        out[write_pos] = '\0';
    }

    if (write_pos == 0U)
    {
        return 0U;
    }

    {
        uint16_t start = 0U;
        uint16_t end = (uint16_t)write_pos;

        DataApp_QuoteTrimLineBounds(out, &start, &end);
        if (end <= start)
        {
            out[0] = '\0';
            return 0U;
        }

        if (start > 0U || end < write_pos)
        {
            memmove(out, &out[start], (size_t)(end - start));
            out[end - start] = '\0';
        }
    }

    return (out[0] != '\0') ? 1U : 0U;
}

static uint8_t DataApp_QuoteSelectRandomLineLocked(uint16_t *picked_index)
{
    uint16_t pick;
    uint8_t tries;

    if (picked_index == NULL || s_quote_runtime.line_count == 0U)
    {
        return 0U;
    }

    if (s_quote_runtime.line_count == 1U)
    {
        *picked_index = 0U;
        s_quote_runtime.last_pick = 0U;
        return 1U;
    }

    for (tries = 0U; tries < 4U; tries++)
    {
        pick = (uint16_t)((uint32_t)rand() % s_quote_runtime.line_count);
        if (pick != s_quote_runtime.last_pick)
        {
            *picked_index = pick;
            s_quote_runtime.last_pick = pick;
            return 1U;
        }
    }

    pick = (uint16_t)((s_quote_runtime.last_pick + 1U) % s_quote_runtime.line_count);
    *picked_index = pick;
    s_quote_runtime.last_pick = pick;
    return 1U;
}

static uint32_t DataApp_QuoteDurationForWidth(uint16_t width_px)
{
    uint16_t scroll_span = 0U;
    uint32_t duration = DATA_APP_QUOTE_SCROLL_HOLD_MS + (DATA_APP_QUOTE_SCROLL_EDGE_PAUSE_MS * 2U);

    if (width_px > DATA_APP_QUOTE_VIEW_W)
    {
        scroll_span = (uint16_t)(width_px - DATA_APP_QUOTE_VIEW_W);
        duration = DATA_APP_QUOTE_SCROLL_HOLD_MS +
                   ((uint32_t)scroll_span * DATA_APP_QUOTE_SCROLL_STEP_MS * 2U) +
                   (DATA_APP_QUOTE_SCROLL_EDGE_PAUSE_MS * 2U);
    }

    if (duration < DATA_APP_QUOTE_MIN_DURATION_MS)
    {
        duration = DATA_APP_QUOTE_MIN_DURATION_MS;
    }
    if (duration > DATA_APP_QUOTE_MAX_DURATION_MS)
    {
        duration = DATA_APP_QUOTE_MAX_DURATION_MS;
    }

    return duration;
}

static void DataApp_QuoteMeasureBitmap(const uint8_t *bitmap,
                                       uint16_t width_px,
                                       uint16_t *visible_offset_x,
                                       uint16_t *visible_width)
{
    uint16_t min_x = width_px;
    uint16_t max_x = 0U;
    uint8_t found = 0U;
    uint8_t row;
    uint16_t col;

    if (visible_offset_x == NULL || visible_width == NULL)
    {
        return;
    }

    *visible_offset_x = 0U;
    *visible_width = width_px;

    if (bitmap == NULL || width_px == 0U)
    {
        return;
    }

    for (col = 0U; col < width_px; col++)
    {
        for (row = 0U; row < DATA_APP_QUOTE_FONT_H; row++)
        {
            uint16_t byte_index = (uint16_t)(row * DATA_APP_QUOTE_RENDER_STRIDE) + (uint16_t)(col / 8U);
            uint8_t mask = (uint8_t)(0x80U >> (col & 0x07U));

            if ((bitmap[byte_index] & mask) == 0U)
            {
                continue;
            }

            if (!found)
            {
                min_x = col;
                max_x = col;
                found = 1U;
            }
            else
            {
                if (col < min_x)
                {
                    min_x = col;
                }
                if (col > max_x)
                {
                    max_x = col;
                }
            }
        }
    }

    if (found)
    {
        *visible_offset_x = min_x;
        *visible_width = (uint16_t)((max_x - min_x) + 1U);
    }
}

static void DataApp_QuoteClearBitmap(uint8_t *bitmap)
{
    memset(bitmap, 0, DATA_APP_QUOTE_RENDER_BUF_SIZE);
}

static void DataApp_QuoteSetPixel(uint8_t *bitmap, uint16_t x, uint8_t y)
{
    uint16_t byte_index;

    if (bitmap == NULL || x >= DATA_APP_QUOTE_RENDER_MAX_W || y >= DATA_APP_QUOTE_FONT_H)
    {
        return;
    }

    byte_index = (uint16_t)(y * DATA_APP_QUOTE_RENDER_STRIDE) + (uint16_t)(x / 8U);
    bitmap[byte_index] |= (uint8_t)(0x80U >> (x & 0x07U));
}

static void DataApp_QuoteRenderGlyph(uint8_t *bitmap, uint16_t x_offset, const uint8_t glyph[DATA_APP_QUOTE_FONT_BYTES])
{
    uint8_t row;
    uint8_t col;

    for (row = 0; row < DATA_APP_QUOTE_FONT_H; row++)
    {
        uint8_t left = glyph[row * 2U];
        uint8_t right = glyph[(row * 2U) + 1U];

        for (col = 0; col < 8U; col++)
        {
            if ((left & (uint8_t)(0x80U >> col)) != 0U)
            {
                DataApp_QuoteSetPixel(bitmap, (uint16_t)(x_offset + col), row);
            }

            if ((right & (uint8_t)(0x80U >> col)) != 0U)
            {
                DataApp_QuoteSetPixel(bitmap, (uint16_t)(x_offset + 8U + col), row);
            }
        }
    }
}

static int DataApp_QuoteReadGlyph(lfs_t *lfs,
                                  lfs_file_t *file,
                                  uint32_t codepoint,
                                  uint8_t glyph[DATA_APP_QUOTE_FONT_BYTES])
{
    int32_t glyph_index = DataApp_QuoteGlyphIndex(codepoint);
    lfs_soff_t seek_ret;
    lfs_ssize_t read_ret;

    if (glyph_index < 0)
    {
        glyph_index = DataApp_QuoteGlyphIndex('?');
        if (glyph_index < 0)
        {
            memset(glyph, 0, DATA_APP_QUOTE_FONT_BYTES);
            return -1;
        }
    }

    seek_ret = lfs_file_seek(lfs,
                             file,
                             (lfs_soff_t)((uint32_t)glyph_index * DATA_APP_QUOTE_FONT_BYTES),
                             LFS_SEEK_SET);
    if (seek_ret < 0)
    {
        memset(glyph, 0, DATA_APP_QUOTE_FONT_BYTES);
        return -1;
    }

    read_ret = lfs_file_read(lfs, file, glyph, DATA_APP_QUOTE_FONT_BYTES);
    if (read_ret != (lfs_ssize_t)DATA_APP_QUOTE_FONT_BYTES)
    {
        memset(glyph, 0, DATA_APP_QUOTE_FONT_BYTES);
        return -1;
    }

    return 0;
}

static uint8_t DataApp_QuoteBuildPrepared(uint16_t line_index, DataApp_QuotePrepared_t *out)
{
    lfs_t *lfs = lfs_port_get();
    lfs_file_t file;
    struct lfs_file_config cfg;
    uint8_t quote[POPUP_MSG_BUF_LEN];
    uint8_t glyph[DATA_APP_QUOTE_FONT_BYTES];
    const char *cursor;
    uint16_t glyph_idx = 0U;
    int open_ret;
    uint16_t missing_glyphs = 0U;
    uint32_t missing_codepoints[4] = {0};
    uint8_t missing_log_count = 0U;
    uint8_t i;

    if (out == NULL || line_index >= s_quote_runtime.line_count)
    {
        return 0U;
    }

    memset(out, 0, sizeof(*out));
    DataApp_QuoteClearBitmap(out->bitmap);

    if (DataApp_QuoteReadIndexedLine(line_index, (char *)quote, sizeof(quote)) == 0U)
    {
        return 0U;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_quote_file_buf;

    lfs_port_lock();
    open_ret = DataApp_QuoteOpenReadonlyFile(&file, &cfg, DATA_APP_QUOTE_FONT_PATH);
    if (open_ret < 0)
    {
        lfs_port_unlock();
        log_printf("[DataApp] quote font open failed\r\n");
        return 0U;
    }

    cursor = (const char *)quote;
    while (cursor != NULL && *cursor != '\0' && glyph_idx < DATA_APP_QUOTE_RENDER_MAX_GLYPHS)
    {
        uint32_t codepoint = DataApp_QuoteDecodeUtf8(&cursor);

        if (DataApp_QuoteGlyphIndex(codepoint) < 0)
        {
            if (missing_log_count < (uint8_t)(sizeof(missing_codepoints) / sizeof(missing_codepoints[0])))
            {
                missing_codepoints[missing_log_count++] = codepoint;
            }
            missing_glyphs++;
        }

        if (DataApp_QuoteReadGlyph(lfs, &file, codepoint, glyph) == 0)
        {
            DataApp_QuoteRenderGlyph(out->bitmap, (uint16_t)(glyph_idx * DATA_APP_QUOTE_FONT_W), glyph);
        }

        glyph_idx++;
    }

    (void)lfs_file_close(lfs, &file);
    lfs_port_unlock();

    strncpy((char *)out->text, (const char *)quote, POPUP_MSG_BUF_LEN - 1U);
    out->text[POPUP_MSG_BUF_LEN - 1U] = '\0';
    out->bitmap_width = (uint16_t)(glyph_idx * DATA_APP_QUOTE_FONT_W);
    DataApp_QuoteMeasureBitmap(out->bitmap, out->bitmap_width, &out->visible_offset_x, &out->visible_width);
    out->scroll_limit = (out->bitmap_width > DATA_APP_QUOTE_VIEW_W) ?
                        (uint16_t)(out->bitmap_width - DATA_APP_QUOTE_VIEW_W) : 0U;
    out->scroll_hold_ms = DATA_APP_QUOTE_SCROLL_HOLD_MS;
    out->scroll_edge_pause_ms = DATA_APP_QUOTE_SCROLL_EDGE_PAUSE_MS;
    out->duration_ms = DataApp_QuoteDurationForWidth(out->bitmap_width);
    out->scroll_px = 0U;
    out->valid = 1U;

    if (missing_glyphs > 0U)
    {
        log_printf("[DataApp] quote missing glyphs=%u\r\n", (unsigned int)missing_glyphs);
        for (i = 0U; i < missing_log_count; i++)
        {
            log_printf("[DataApp] missing cp[%u]=U+%04lX\r\n",
                       (unsigned int)i,
                       (unsigned long)missing_codepoints[i]);
        }
    }

    return 1U;
}

/**
 * @brief  只执行一次字库文件合法性校验
 * @retval None
 */
static void DataApp_QuoteValidateFontFileOnce(void)
{
    lfs_t *lfs;
    struct lfs_info info;
    int stat_ret;

    if (s_quote_runtime.font_checked)
    {
        return;
    }

    lfs = lfs_port_get();
    lfs_port_lock();
    stat_ret = lfs_stat(lfs, DATA_APP_QUOTE_FONT_PATH, &info);
    if (stat_ret < 0 && DATA_APP_QUOTE_FONT_PATH[0] == '/')
    {
        stat_ret = lfs_stat(lfs, &DATA_APP_QUOTE_FONT_PATH[1], &info);
    }
    lfs_port_unlock();

    if (stat_ret == 0)
    {
        if ((uint32_t)info.size == DATA_APP_QUOTE_FONT_SIZE)
        {
            log_printf("[DataApp] quote font size OK=%lu\r\n", (unsigned long)DATA_APP_QUOTE_FONT_SIZE);
        }
        else
        {
            log_printf("[DataApp] quote font size mismatch exp=%lu actual=%lu\r\n",
                       (unsigned long)DATA_APP_QUOTE_FONT_SIZE,
                       (unsigned long)info.size);
        }
    }
    else
    {
        log_printf("[DataApp] quote font stat failed\r\n");
    }

    s_quote_runtime.font_checked = 1U;
}

static void DataApp_QuoteUpdateScrollLocked(TickType_t now)
{
    DataApp_QuotePrepared_t *active = &s_quote_runtime.active;
    uint32_t elapsed_ms;
    uint32_t hold_ms;
    uint32_t edge_pause_ms;
    uint32_t one_way_scroll_ms;
    uint32_t cycle_ms;
    uint32_t cycle_pos;

    if (!active->valid)
    {
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
        return;
    }

    active->scroll_px = 0U;
    hold_ms = active->scroll_hold_ms;
    edge_pause_ms = active->scroll_edge_pause_ms;

    if (active->scroll_limit == 0U)
    {
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
        return;
    }

    elapsed_ms = (uint32_t)((now - s_quote_runtime.active_start_tick) * portTICK_PERIOD_MS);
    if (elapsed_ms < hold_ms)
    {
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
        return;
    }

    elapsed_ms -= hold_ms;
    one_way_scroll_ms = (uint32_t)active->scroll_limit * DATA_APP_QUOTE_SCROLL_STEP_MS;
    cycle_ms = (one_way_scroll_ms * 2U) + (edge_pause_ms * 2U);
    if (cycle_ms == 0U)
    {
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
        return;
    }

    cycle_pos = elapsed_ms % cycle_ms;
    if (cycle_pos < one_way_scroll_ms)
    {
        uint32_t scroll_px = cycle_pos / DATA_APP_QUOTE_SCROLL_STEP_MS;

        if (scroll_px > active->scroll_limit)
        {
            scroll_px = active->scroll_limit;
        }

        active->scroll_px = (uint16_t)scroll_px;
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_LEFT;
        return;
    }

    cycle_pos -= one_way_scroll_ms;
    if (cycle_pos < edge_pause_ms)
    {
        active->scroll_px = active->scroll_limit;
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_LEFT_PAUSE;
        return;
    }

    cycle_pos -= edge_pause_ms;
    if (cycle_pos < one_way_scroll_ms)
    {
        uint32_t scroll_px = cycle_pos / DATA_APP_QUOTE_SCROLL_STEP_MS;

        if (scroll_px > active->scroll_limit)
        {
            scroll_px = active->scroll_limit;
        }

        active->scroll_px = (uint16_t)(active->scroll_limit - (uint16_t)scroll_px);
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_RIGHT;
        return;
    }

    active->scroll_px = 0U;
    s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_RIGHT_PAUSE;
}

static unsigned int DataApp_QuoteBuildSeed(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    uint32_t seed = HAL_GetTick();

    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK &&
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) == HAL_OK)
    {
        seed ^= ((uint32_t)sTime.Hours << 24);
        seed ^= ((uint32_t)sTime.Minutes << 16);
        seed ^= ((uint32_t)sTime.Seconds << 8);
        seed ^= ((uint32_t)sDate.Date << 4);
        seed ^= ((uint32_t)sDate.Month << 1);
        seed ^= (uint32_t)sDate.Year;
    }

    if (seed == 0U)
    {
        seed = 0x13572468U;
    }

    return (unsigned int)seed;
}

/**
 * @brief  初始化语录与时间数据层运行时
 * @retval None
 */
void DataApp_Init(void)
{
    // 这里只做“数据层运行时”初始化，不主动触发弹窗显示。
    // 真正的页面展示由 AppDataTask + UI 层根据时机决定。
    DataApp_QuoteResetRuntime();

#if FLASH_USE_FREERTOS
    if (s_data_app_lock == NULL)
    {
        s_data_app_lock = xSemaphoreCreateMutex();
        if (s_data_app_lock == NULL)
        {
            log_printf("[DataApp] mutex create FAIL\r\n");
        }
    }
#endif

    srand(DataApp_QuoteBuildSeed());
    s_quote_runtime.initialized = 1U;

    // 资源固化，开机即加载语录索引和字库，无需运行时预热。
    DataApp_QuoteValidateFontFileOnce();
    DataApp_QuoteLoadCache();
}

/**
 * @brief  当语录资源变化时清理缓存
 * @retval None
 */
void DataApp_QuoteInvalidate(void)
{
    // 当文件系统资源变化或需要强制重扫语录文件时，可调用这里清缓存。
    if (!s_quote_runtime.initialized)
    {
        return;
    }

    DataApp_Lock();
    memset(s_quote_lines, 0, sizeof(s_quote_lines));
    memset(&s_quote_runtime.active, 0, sizeof(s_quote_runtime.active));
    memset(&s_quote_runtime.pending, 0, sizeof(s_quote_runtime.pending));
    s_quote_runtime.active_start_tick = 0U;
    s_quote_runtime.last_refresh_check_tick = 0U;
    s_quote_runtime.file_size = 0U;
    s_quote_runtime.line_count = 0U;
    s_quote_runtime.cache_loaded = 0U;
    s_quote_runtime.last_pick = UINT16_MAX;
    s_quote_runtime.font_checked = 0U;
    s_quote_runtime.active_started_showing = 0U;
    DataApp_Unlock();

    log_printf("[DataApp] quote cache invalidated\r\n");
}

/**
 * @brief  预热语录索引和字库
 * @retval None
 */
void DataApp_QuoteWarmup(void)
{
}

/**
 * @brief  语录调度更新（精简版）：仅处理 active 语录的滚动更新
 * @param  now: 当前 tick
 * @retval None
 */
void DataApp_QuoteServiceUpdate(TickType_t now)
{
    if (!s_quote_runtime.initialized || !s_quote_runtime.cache_loaded)
    {
        return;
    }

    DataApp_Lock();

    if (s_quote_runtime.active.valid)
    {
        DataApp_QuoteUpdateScrollLocked(now);

        if (UI_Popup_IsLowShowing())
        {
            s_quote_runtime.active_started_showing = 1U;
        }
        else if (s_quote_runtime.active_started_showing)
        {
            memset(&s_quote_runtime.active, 0, sizeof(s_quote_runtime.active));
            s_quote_runtime.active_start_tick = 0U;
            s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
            s_quote_runtime.active_started_showing = 0U;
        }
    }

    DataApp_Unlock();
}

/**
 * @brief  Monitor 定时器回调中调用：选行、渲染、设 active、返回弹窗请求
 * @param  out: 输出弹窗文本和时长
 * @retval 1 表示成功
 */
uint8_t DataApp_QuoteShowNext(DataApp_QuotePopupRequest_t *out)
{
    uint16_t picked_index = 0U;
    DataApp_QuotePrepared_t *prepared = &s_quote_prepare_scratch;

    if (out == NULL)
    {
        return 0U;
    }

    if (!s_quote_runtime.initialized || !s_quote_runtime.cache_loaded)
    {
        return 0U;
    }

    memset(prepared, 0, sizeof(*prepared));

    DataApp_Lock();
    if (s_quote_runtime.active.valid || s_quote_runtime.line_count == 0U)
    {
        DataApp_Unlock();
        return 0U;
    }
    if (!DataApp_QuoteSelectRandomLineLocked(&picked_index))
    {
        DataApp_Unlock();
        return 0U;
    }
    DataApp_Unlock();

    /* 无锁执行渲染（LittleFS 读取 + 字模渲染，耗时不确定）。 */
    if (!DataApp_QuoteBuildPrepared(picked_index, prepared))
    {
        return 0U;
    }

    DataApp_Lock();
    if (s_quote_runtime.active.valid)
    {
        DataApp_Unlock();
        return 0U;
    }

    s_quote_runtime.active = *prepared;
    s_quote_runtime.active.valid = 1U;
    s_quote_runtime.active_start_tick = xTaskGetTickCount();
    s_quote_runtime.active.scroll_px = 0U;
    s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
    s_quote_runtime.active_started_showing = 0U;
    DataApp_Unlock();

    strncpy(out->text, prepared->text, POPUP_MSG_BUF_LEN - 1U);
    out->text[POPUP_MSG_BUF_LEN - 1U] = '\0';
    out->duration_ms = prepared->duration_ms;

    return 1U;
}

/**
 * @brief  读取待展示语录请求
 * @param  out: 输出请求
 * @retval 1 表示有待展示内容
 */
uint8_t DataApp_QuotePopup_PeekPending(DataApp_QuotePopupRequest_t *out)
{
    uint8_t ret = 0U;

    // 只读查看是否有待展示语录，不改变 active/pending 状态。
    if (out == NULL)
    {
        return 0U;
    }

    DataApp_Lock();
    if (s_quote_runtime.pending.valid)
    {
        strncpy(out->text, s_quote_runtime.pending.text, POPUP_MSG_BUF_LEN - 1U);
        out->text[POPUP_MSG_BUF_LEN - 1U] = '\0';
        out->duration_ms = s_quote_runtime.pending.duration_ms;
        ret = 1U;
    }
    DataApp_Unlock();

    return ret;
}

/**
 * @brief  将 pending 语录升级为 active
 * @param  now: 当前 tick
 * @retval None
 */
void DataApp_QuotePopup_CommitPending(TickType_t now)
{
    // 当 UI 层确认已经把 pending 语录送入弹窗队列后，
    // 在这里把它提升为 active，后续滚动位移就以它为准。
    DataApp_Lock();
    if (s_quote_runtime.pending.valid)
    {
        s_quote_runtime.active = s_quote_runtime.pending;
        s_quote_runtime.active.valid = 1U;
        s_quote_runtime.active_start_tick = now;
        s_quote_runtime.active.scroll_px = 0U;
        s_quote_runtime.active_scroll_phase = DATA_APP_QUOTE_SCROLL_PHASE_HOLD;
        s_quote_runtime.pending.valid = 0U;
        s_quote_runtime.active_started_showing = 0U;
    }
    DataApp_Unlock();
}

/**
 * @brief  复制当前弹窗帧快照
 * @param  out: 输出帧
 * @retval 1 表示成功
 */
uint8_t DataApp_QuotePopup_CopyFrame(DataApp_QuotePopupFrame_t *out)
{
    uint8_t ret = 0U;

    // 弹窗绘制层通过复制快照拿到当前帧数据，避免直接访问运行时内部结构。
    if (out == NULL)
    {
        return 0U;
    }

    DataApp_Lock();
    if (s_quote_runtime.active.valid)
    {
        memset(out, 0, sizeof(*out));
        strncpy(out->text, s_quote_runtime.active.text, POPUP_MSG_BUF_LEN - 1U);
        out->text[POPUP_MSG_BUF_LEN - 1U] = '\0';
        memcpy(out->bitmap, s_quote_runtime.active.bitmap, sizeof(out->bitmap));
        out->bitmap_width = s_quote_runtime.active.bitmap_width;
        out->visible_offset_x = s_quote_runtime.active.visible_offset_x;
        out->visible_width = s_quote_runtime.active.visible_width;
        out->scroll_px = s_quote_runtime.active.scroll_px;
        out->scroll_limit = s_quote_runtime.active.scroll_limit;
        out->scroll_hold_ms = s_quote_runtime.active.scroll_hold_ms;
        out->scroll_edge_pause_ms = s_quote_runtime.active.scroll_edge_pause_ms;
        out->duration_ms = s_quote_runtime.active.duration_ms;
        out->valid = 1U;
        ret = 1U;
    }
    DataApp_Unlock();

    return ret;
}

TiltKey_t TiltKey_Update(motion_module_t *motion)
{
    enum {
        TILT_SHAKE_AXIS_NONE = 0,
        TILT_SHAKE_AXIS_HORIZONTAL,
        TILT_SHAKE_AXIS_VERTICAL
    };
    const TickType_t shake_window_ticks = pdMS_TO_TICKS(2000U);
    const TickType_t shake_lock_ticks = pdMS_TO_TICKS(600U);
    const TickType_t single_tilt_delay_ticks = pdMS_TO_TICKS(400U);
    uint8_t buf = motion->buf_idx;
    int16_t raw_x = motion->x[buf];
    int16_t raw_y = motion->y[buf];
    int16_t raw_z = motion->z[buf];

    float ax = raw_x * 0.001f;
    float ay = raw_y * 0.001f;
    float az = raw_z * 0.001f;

    static float fx = 0, fy = 0, fz = 0;
    fx = fx * 0.7f + ax * 0.3f;
    fy = fy * 0.7f + ay * 0.3f;
    fz = fz * 0.7f + az * 0.3f;

    float pitch = atan2f(fx, sqrtf(fy * fy + fz * fz)) * RAD2DEG;
    float roll  = atan2f(fy, sqrtf(fx * fx + fz * fz)) * RAD2DEG;

    current_raw_direction = MSG_TILT_NONE;

    if (pitch > ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_LEFT;
    }
    else if (pitch < -ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_RIGHT;
    }
    else if (roll > ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_UP;
    }
    else if (roll < -ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_DOWN;
    }

    static TiltKey_t last_stable_key = MSG_TILT_NONE;
    static uint8_t stable_cnt = 0;
    static TiltKey_t output_locked_key = MSG_TILT_NONE;
    static TiltKey_t last_shake_horizontal_key = MSG_TILT_NONE;
    static TiltKey_t last_shake_vertical_key = MSG_TILT_NONE;
    static uint8_t horizontal_shake_cnt = 0;
    static uint8_t vertical_shake_cnt = 0;
    static TickType_t horizontal_shake_start = 0;
    static TickType_t vertical_shake_start = 0;
    static TickType_t horizontal_shake_locked_until = 0;
    static TickType_t vertical_shake_locked_until = 0;
    static TiltKey_t pending_output_key = MSG_TILT_NONE;
    static TickType_t pending_output_start = 0;

    if (current_raw_direction == last_stable_key)
    {
        if (stable_cnt < HOLD_COUNT)
        {
            stable_cnt++;
        }
    }
    else
    {
        stable_cnt = 0;
        last_stable_key = current_raw_direction;
    }

    if (stable_cnt >= HOLD_COUNT)
    {
        if (current_raw_direction != MSG_TILT_NONE)
        {
            TickType_t now = xTaskGetTickCount();
            uint8_t axis = TILT_SHAKE_AXIS_NONE;
            TiltKey_t shake_event = MSG_TILT_NONE;

            if (current_raw_direction == MSG_TILT_LEFT || current_raw_direction == MSG_TILT_RIGHT)
            {
                axis = TILT_SHAKE_AXIS_HORIZONTAL;
            }
            else if (current_raw_direction == MSG_TILT_UP || current_raw_direction == MSG_TILT_DOWN)
            {
                axis = TILT_SHAKE_AXIS_VERTICAL;
            }

            if (axis == TILT_SHAKE_AXIS_HORIZONTAL)
            {
                if (horizontal_shake_locked_until != 0U &&
                    (TickType_t)(now - horizontal_shake_locked_until) < shake_lock_ticks)
                {
                    return MSG_TILT_NONE;
                }

                if (last_shake_horizontal_key == MSG_TILT_NONE)
                {
                    last_shake_horizontal_key = current_raw_direction;
                    horizontal_shake_cnt = 1U;
                    horizontal_shake_start = now;
                }
                else if (current_raw_direction != last_shake_horizontal_key)
                {
                    if ((TickType_t)(now - horizontal_shake_start) > shake_window_ticks)
                    {
                        horizontal_shake_cnt = 1U;
                        horizontal_shake_start = now;
                    }
                    else if (horizontal_shake_cnt < 255U)
                    {
                        horizontal_shake_cnt++;
                    }

                    last_shake_horizontal_key = current_raw_direction;

                    if (horizontal_shake_cnt >= 3U)
                    {
                        shake_event = MSG_TILT_SHAKE_HORIZONTAL;
                        horizontal_shake_cnt = 0U;
                        last_shake_horizontal_key = MSG_TILT_NONE;
                        horizontal_shake_start = now;
                        horizontal_shake_locked_until = now;
                    }
                }
            }
            else if (axis == TILT_SHAKE_AXIS_VERTICAL)
            {
                if (vertical_shake_locked_until != 0U &&
                    (TickType_t)(now - vertical_shake_locked_until) < shake_lock_ticks)
                {
                    return MSG_TILT_NONE;
                }

                if (last_shake_vertical_key == MSG_TILT_NONE)
                {
                    last_shake_vertical_key = current_raw_direction;
                    vertical_shake_cnt = 1U;
                    vertical_shake_start = now;
                }
                else if (current_raw_direction != last_shake_vertical_key)
                {
                    if ((TickType_t)(now - vertical_shake_start) > shake_window_ticks)
                    {
                        vertical_shake_cnt = 1U;
                        vertical_shake_start = now;
                    }
                    else if (vertical_shake_cnt < 255U)
                    {
                        vertical_shake_cnt++;
                    }

                    last_shake_vertical_key = current_raw_direction;

                    if (vertical_shake_cnt >= 3U)
                    {
                        shake_event = MSG_TILT_SHAKE_VERTICAL;
                        vertical_shake_cnt = 0U;
                        last_shake_vertical_key = MSG_TILT_NONE;
                        vertical_shake_start = now;
                        vertical_shake_locked_until = now;
                    }
                }
            }

            if (shake_event != MSG_TILT_NONE)
            {
                output_locked_key = shake_event;
                pending_output_key = MSG_TILT_NONE;
                return shake_event;
            }

            if (output_locked_key == MSG_TILT_NONE)
            {
                if (pending_output_key == MSG_TILT_NONE)
                {
                    pending_output_key = current_raw_direction;
                    pending_output_start = now;
                }
                else if (pending_output_key != current_raw_direction)
                {
                    pending_output_key = MSG_TILT_NONE;
                    pending_output_start = now;
                }
                else if ((TickType_t)(now - pending_output_start) >= single_tilt_delay_ticks)
                {
                    output_locked_key = current_raw_direction;
                    pending_output_key = MSG_TILT_NONE;
                    return current_raw_direction;
                }
            }
        }
    }

    if (fabs(pitch) < ANGLE_HYST && fabs(roll) < ANGLE_HYST)
    {
        output_locked_key = MSG_TILT_NONE;
        last_shake_horizontal_key = MSG_TILT_NONE;
        last_shake_vertical_key = MSG_TILT_NONE;
        pending_output_key = MSG_TILT_NONE;
    }

    return MSG_TILT_NONE;
}

TiltKey_t FallDetect_Check(motion_module_t *motion)
{
    uint8_t buf = motion->buf_idx;
    float ax = motion->x[buf] * 0.001f;
    float ay = motion->y[buf] * 0.001f;
    float az = motion->z[buf] * 0.001f;

    static float fx = 0, fy = 0, fz = 0;
    fx = fx * 0.7f + ax * 0.3f;
    fy = fy * 0.7f + ay * 0.3f;
    fz = fz * 0.7f + az * 0.3f;

    float acc_mag = sqrtf(fx * fx + fy * fy + fz * fz);

    static uint8_t fall_cnt = 0;
    static uint16_t init_cnt = 0;
    static uint8_t fall_trig_lock = 0;

    if (init_cnt < 20)
    {
        init_cnt++;
        fall_cnt = 0;
        fall_trig_lock = 0;
        return MSG_TILT_NONE;
    }

    if (acc_mag < 0.3f || acc_mag > 1.5f)
    {
        fall_cnt = 0;
        return MSG_TILT_NONE;
    }

    if (acc_mag < FALL_THRESHOLD)
    {
        fall_cnt++;
        if (fall_cnt >= FALL_TRIG_CNT && !fall_trig_lock)
        {
            fall_trig_lock = 1;
            fall_cnt = 0;
            return MSG_FALL_DOWN;
        }
    }
    else
    {
        fall_cnt = 0;
        fall_trig_lock = 0;
    }

    return MSG_TILT_NONE;
}
