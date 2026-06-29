#ifndef __LED_APP_H__
#define __LED_APP_H__


#include "rgb_led.h"

typedef enum {
    LED_EVT_NONE = 0,
    LED_EVT_ON,
    LED_EVT_STATIC_RED,
    LED_EVT_STATIC_GREEN,
    LED_EVT_STATIC_BLUE,
    LED_EVT_BLINK_RED,
    LED_EVT_BLINK_GREEN,
    LED_EVT_BLINK_BLUE,
    LED_EVT_HEARTBEAT_RED,
    LED_EVT_HEARTBEAT_GREEN,
    LED_EVT_HEARTBEAT_BLUE,
    LED_EVT_BREATH_RED,
    LED_EVT_BREATH_GREEN,
    LED_EVT_BREATH_BLUE,
    LED_EVT_RAINBOW,
    LED_EVT_STOP,
} LED_EVT_t;

 
extern LED_Object_t led_blue;  // PE3 -> TIM3_CH1
extern LED_Object_t led_green; // PE4 -> TIM3_CH2
extern LED_Object_t led_red;   // PE5 -> TIM3_CH3

extern RGB_Object_t rgb;

void LED_CMD_OnEvent(LED_EVT_t event);

#endif /* __LED_APP_H__ */
