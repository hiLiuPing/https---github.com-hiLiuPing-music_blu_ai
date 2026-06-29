#include "led_app.h"

LED_Object_t led_blue;  // PE3 -> TIM3_CH1
LED_Object_t led_green; // PE4 -> TIM3_CH2
LED_Object_t led_red;   // PE5 -> TIM3_CH3

RGB_Object_t rgb;

void LED_CMD_OnEvent(LED_EVT_t event)
{

    switch (event)
    {
    case LED_EVT_ON:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_WHITE, 0);
        break;
  case LED_EVT_STATIC_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_STATIC_GREEN:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_STATIC_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_BLUE, 0);
        break;
    case LED_EVT_BLINK_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_BLINK, 1000, 0, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_BLINK_GREEN:
        RGB_SendCmd(&rgb, RGB_EFFECT_BLINK, 1000, 0, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_BLINK_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_BLINK, 1000, 0, RGB_COLOR_BLUE, 0);
        break;
    case LED_EVT_HEARTBEAT_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_HEARTBEAT, 2000, 2000, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_HEARTBEAT_GREEN:

      RGB_SendCmd(&rgb, RGB_EFFECT_HEARTBEAT, 2000, 2000, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_HEARTBEAT_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_HEARTBEAT, 2000, 2000, RGB_COLOR_BLUE, 0);
        break;
     case LED_EVT_BREATH_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_BREATH, 5000, 0, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_BREATH_GREEN:
        RGB_SendCmd(&rgb, RGB_EFFECT_BREATH, 5000, 0, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_BREATH_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_BREATH, 5000, 0, RGB_COLOR_BLUE, 0);
        break;
    case LED_EVT_RAINBOW:
       RGB_SendCmd(&rgb, RGB_EFFECT_RAINBOW, 5000, 0, 0, 0);
        break;
    case LED_EVT_STOP:
        RGB_Stop(&rgb);
        break;

    default:
        break;
    }
}
