#include "oled_anim.h"
#include <math.h>

/*
 * 基础动画曲线和组件状态机。
 * 这里只处理“位置如何从 A 平滑移动到 B”，不关心具体画什么。
 */

float Linear(float t) { return t; }

float EaseOutCubic(float t)
{
    t -= 1.0f;
    return t * t * t + 1.0f;
}

float EaseOutBack(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float t1 = t - 1.0f;
    return 1.0f + c3 * (t1 * t1 * t1) + c1 * (t1 * t1);
}

float EaseInBack(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * (t * t * t) - c1 * (t * t);
}

float EaseOutBounce(float t)
{
    if (t < (1 / 2.75f)) return 7.5625f * t * t;
    else if (t < (2 / 2.75f)) return 7.5625f * (t -= (1.5f / 2.75f)) * t + 0.75f;
    else if (t < (2.5 / 2.75f)) return 7.5625f * (t -= (2.25f / 2.75f)) * t + 0.9375f;
    else return 7.5625f * (t -= (2.625f / 2.75f)) * t + 0.984375f;
}

/**
 * @brief  更新动画组件的位置
 * @param  comp: 动画组件
 * @retval None
 */
void UI_Comp_Update(UI_Comp_t *comp)
{
    if (comp->state == UI_STATE_IDLE || comp->state == UI_STATE_DEAD) return;

    comp->t += comp->speed;
    if (comp->t >= 1.0f)
    {
        comp->t = 1.0f;
        comp->x = comp->target_x;
        comp->y = comp->target_y;
        comp->state = (comp->state == UI_STATE_EXIT) ? UI_STATE_DEAD : UI_STATE_IDLE;
    }
    else
    {
        float val = comp->easing_func(comp->t);
        comp->x = comp->start_x + (comp->target_x - comp->start_x) * val;
        comp->y = comp->start_y + (comp->target_y - comp->start_y) * val;
    }
}

/**
 * @brief  设置动画组件的起点、终点和缓动曲线
 * @param  comp: 动画组件
 * @retval None
 */
void UI_Comp_SetLayout(UI_Comp_t *comp, float sx, float sy, float tx, float ty, float speed, EasingFunc_t func)
{
    comp->start_x = sx;
    comp->start_y = sy;
    comp->target_x = tx;
    comp->target_y = ty;
    comp->x = sx;
    comp->y = sy;
    comp->speed = speed;
    comp->t = 0.0f;
    comp->easing_func = func;
    comp->state = UI_STATE_MOVE;
}
