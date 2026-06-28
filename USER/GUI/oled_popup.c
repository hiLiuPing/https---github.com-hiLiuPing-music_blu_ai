#include <string.h>

#include "oled_popup.h"
#include "systemMonitor_app.h"
#include "queue.h"

/*
 * 双优先级弹窗状态机。
 * 高优先级（g_popup）用于系统事件弹窗，低优先级（g_popup_low）用于语录等。
 * 渲染时先低后高，高优先级始终覆盖在低优先级之上。
 */

static UI_Popup_t g_popup;
static QueueHandle_t xPopupQueue = NULL;

static UI_Popup_t g_popup_low;
static QueueHandle_t xPopupQueueLow = NULL;
static const void *g_popup_low_runtime_data = NULL;

/**
 * @brief  初始化弹窗队列和运行时
 * @retval None
 */
void UI_Popup_Init(void)
{
    memset(&g_popup, 0, sizeof(g_popup));
    xPopupQueue = xQueueCreate(10, sizeof(PopupTask_t));

    memset(&g_popup_low, 0, sizeof(g_popup_low));
    xPopupQueueLow = xQueueCreate(10, sizeof(PopupTask_t));
}

/**
 * @brief  入队一个高优先级弹窗请求
 * @retval 1 表示入队成功
 */
uint8_t UI_Popup_Show(PopupDrawFunc draw_cb, const char *msg, uint32_t ms)
{
    PopupTask_t task;

    if (xPopupQueue == NULL || draw_cb == NULL || msg == NULL)
    {
        return 0U;
    }

    memset(&task, 0, sizeof(task));
    task.draw_cb = draw_cb;
    task.data = msg;
    task.ms = ms;

    return (xQueueSend(xPopupQueue, &task, 0) == pdPASS) ? 1U : 0U;
}

/**
 * @brief  入队一个低优先级弹窗请求
 * @retval 1 表示入队成功
 */
uint8_t UI_Popup_ShowLow(PopupDrawFunc draw_cb, const char *msg, uint32_t ms)
{
    PopupTask_t task;

    if (xPopupQueueLow == NULL || draw_cb == NULL || msg == NULL)
    {
        return 0U;
    }

    memset(&task, 0, sizeof(task));
    task.draw_cb = draw_cb;
    task.data = msg;
    task.ms = ms;

    return (xQueueSend(xPopupQueueLow, &task, 0) == pdPASS) ? 1U : 0U;
}

/**
 * @brief  推进高优先级弹窗生命周期
 */
static void Popup_UpdateHigh(void)
{
    if (xPopupQueue == NULL)
    {
        return;
    }

    if (!g_popup.is_active)
    {
        PopupTask_t next_task;

        if (xQueueReceive(xPopupQueue, &next_task, 0) == pdPASS)
        {
            g_popup.draw_cb = next_task.draw_cb;
            if (next_task.data != NULL)
            {
                strncpy(g_popup.msg_buf, (const char *)next_task.data, POPUP_MSG_BUF_LEN - 1U);
                g_popup.msg_buf[POPUP_MSG_BUF_LEN - 1U] = '\0';
            }
            else
            {
                g_popup.msg_buf[0] = '\0';
            }

            g_popup.timer = next_task.ms / 16U;
            if (g_popup.timer == 0U)
            {
                g_popup.timer = 1U;
            }

            g_popup.start_tick = xTaskGetTickCount();
            g_popup.is_active = 1U;
            UI_Comp_SetLayout(&g_popup.base, 0.0f, -32.0f, 0.0f, 0.0f, 0.08f, EaseOutBack);
        }

        return;
    }

    UI_Comp_Update(&g_popup.base);

    if (g_popup.base.state == UI_STATE_IDLE)
    {
        if (g_popup.timer > 0U)
        {
            g_popup.timer--;
        }
        else
        {
            UI_Comp_SetLayout(&g_popup.base, 0.0f, 0.0f, 0.0f, -32.0f, 0.1f, EaseInBack);
            g_popup.base.state = UI_STATE_EXIT;
        }
    }

    if (g_popup.base.state == UI_STATE_DEAD)
    {
        memset(&g_popup.base, 0, sizeof(g_popup.base));
        g_popup.draw_cb = NULL;
        g_popup.msg_buf[0] = '\0';
        g_popup.start_tick = 0;
        g_popup.is_active = 0U;
    }
}

/**
 * @brief  推进低优先级弹窗生命周期
 */
static void Popup_UpdateLow(void)
{
    if (xPopupQueueLow == NULL)
    {
        return;
    }

    if (!g_popup_low.is_active)
    {
        PopupTask_t next_task;

        if (xQueueReceive(xPopupQueueLow, &next_task, 0) == pdPASS)
        {
            g_popup_low.draw_cb = next_task.draw_cb;
            if (next_task.data != NULL)
            {
                strncpy(g_popup_low.msg_buf, (const char *)next_task.data, POPUP_MSG_BUF_LEN - 1U);
                g_popup_low.msg_buf[POPUP_MSG_BUF_LEN - 1U] = '\0';
            }
            else
            {
                g_popup_low.msg_buf[0] = '\0';
            }

            g_popup_low.timer = next_task.ms / 16U;
            if (g_popup_low.timer == 0U)
            {
                g_popup_low.timer = 1U;
            }

            g_popup_low.start_tick = xTaskGetTickCount();
            g_popup_low.is_active = 1U;
            UI_Comp_SetLayout(&g_popup_low.base, 0.0f, -32.0f, 0.0f, 0.0f, 0.08f, EaseOutBack);
        }

        return;
    }

    UI_Comp_Update(&g_popup_low.base);

    if (g_popup_low.base.state == UI_STATE_IDLE)
    {
        if (g_popup_low.timer > 0U)
        {
            g_popup_low.timer--;
        }
        else
        {
            UI_Comp_SetLayout(&g_popup_low.base, 0.0f, 0.0f, 0.0f, -32.0f, 0.1f, EaseInBack);
            g_popup_low.base.state = UI_STATE_EXIT;
        }
    }

    if (g_popup_low.base.state == UI_STATE_DEAD)
    {
        memset(&g_popup_low.base, 0, sizeof(g_popup_low.base));
        g_popup_low.draw_cb = NULL;
        g_popup_low.msg_buf[0] = '\0';
        g_popup_low.start_tick = 0;
        g_popup_low.is_active = 0U;
    }
}

/**
 * @brief  推进所有优先级弹窗生命周期
 * @retval None
 */
void UI_Popup_Update(void)
{
    Popup_UpdateHigh();
    Popup_UpdateLow();
}

/**
 * @brief  绘制所有弹窗（先低后高，高优先级覆盖）
 * @param  u8g2: 渲染句柄
 * @retval None
 */
void UI_Popup_Render(u8g2_t *u8g2)
{
    if (xPopupQueue == NULL)
    {
        return;
    }

    /* 低优先级（底层）：语录等 */
    if (g_popup_low.is_active && g_popup_low.draw_cb != NULL)
    {
        g_popup_low.draw_cb(u8g2, &g_popup_low.base, (void *)g_popup_low_runtime_data);
    }

    /* 高优先级（顶层）：系统事件弹窗 */
    if (g_popup.is_active && g_popup.draw_cb != NULL)
    {
        g_popup.draw_cb(u8g2, &g_popup.base, g_popup.msg_buf);
    }
}

uint8_t UI_Popup_IsBusy(void)
{
    if (g_popup.is_active)
    {
        return 1U;
    }

    if (xPopupQueue == NULL)
    {
        return 0U;
    }

    return (uxQueueMessagesWaiting(xPopupQueue) > 0U) ? 1U : 0U;
}

uint8_t UI_Popup_IsBusyLow(void)
{
    if (g_popup_low.is_active)
    {
        return 1U;
    }

    if (xPopupQueueLow == NULL)
    {
        return 0U;
    }

    return (uxQueueMessagesWaiting(xPopupQueueLow) > 0U) ? 1U : 0U;
}

uint8_t UI_Popup_IsShowing(void)
{
    return g_popup.is_active ? 1U : 0U;
}

uint8_t UI_Popup_IsLowShowing(void)
{
    return g_popup_low.is_active ? 1U : 0U;
}

void UI_Popup_SetLowRuntimeData(const void *data)
{
    g_popup_low_runtime_data = data;
}

uint32_t UI_Popup_GetElapsedMs(void)
{
    if (!g_popup.is_active)
    {
        return 0U;
    }

    return (uint32_t)((xTaskGetTickCount() - g_popup.start_tick) * portTICK_PERIOD_MS);
}
