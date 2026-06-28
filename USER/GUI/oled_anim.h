#ifndef __OLED_ANIM_H__
#define __OLED_ANIM_H__

/*
 * UI 缓动与简单动画组件。
 * 页面切换、组件位移动画都复用这里的 easing 函数和状态机。
 */

/* 定义缓动函数指针类型。 */
typedef float (*EasingFunc_t)(float);

/* 常用缓动曲线。 */
float Linear(float t);
float EaseOutCubic(float t);
float EaseInBack(float t);
float EaseOutBack(float t);
float EaseOutBounce(float t);

/* 组件状态。 */
typedef enum {
    UI_STATE_IDLE,
    UI_STATE_MOVE,
    UI_STATE_EXIT,
    UI_STATE_DEAD
} UI_State_t;

/* 动画组件上下文。 */
typedef struct {
    float x, y;
    float target_x, target_y;
    float start_x, start_y;
    float t;
    float speed;
    EasingFunc_t easing_func;
    UI_State_t state;
} UI_Comp_t;

/* 动画控制接口。 */
void UI_Comp_SetLayout(UI_Comp_t *comp, float sx, float sy, float tx, float ty, float speed, EasingFunc_t func);
void UI_Comp_Update(UI_Comp_t *comp);

#endif
