#include "bsp_key.h"
#include "ti_msp_dl_config.h"

#define KEY_DEBOUNCE_MS 20

uint8_t keyK1Pressed(uint32_t now)
{
    static uint8_t state = 0;
    static uint32_t change_time = 0;

    uint8_t pressed =
        (DL_GPIO_readPins(KEY_PORT, KEY_K1_PIN) == 0);

    switch (state)
    {
        case 0:  // 等待按下
            if (pressed)
            {
                change_time = now;
                state = 1;
            }
            break;

        case 1:  // 按下消抖
            if (!pressed)
            {
                state = 0;
            }
            else if ((now - change_time)
                     >= KEY_DEBOUNCE_MS)
            {
                state = 2;
                return 1;
            }
            break;

        case 2:  // 等待松开
            if (!pressed)
            {
                change_time = now;
                state = 3;
            }
            break;

        case 3:  // 松开消抖
            if (pressed)
            {
                state = 2;
            }
            else if ((now - change_time)
                     >= KEY_DEBOUNCE_MS)
            {
                state = 0;
            }
            break;

        default:
            state = 0;
            break;
    }

    return 0;
}


uint8_t keyK2Pressed(uint32_t now)
{
    static uint8_t state = 0;
    static uint32_t change_time = 0;

    /* PB19低电平表示按下 */
    uint8_t pressed =
        (DL_GPIO_readPins(KEY2_PORT, KEY2_K2_PIN) == 0);

    switch (state)
    {
        case 0:  // 等待按下
            if (pressed)
            {
                change_time = now;
                state = 1;
            }
            break;

        case 1:  // 按下消抖
            if (!pressed)
            {
                state = 0;
            }
            else if ((now - change_time)
                     >= KEY_DEBOUNCE_MS)
            {
                state = 2;
                return 1;
            }
            break;

        case 2:  // 等待松开
            if (!pressed)
            {
                change_time = now;
                state = 3;
            }
            break;

        case 3:  // 松开消抖
            if (pressed)
            {
                state = 2;
            }
            else if ((now - change_time)
                     >= KEY_DEBOUNCE_MS)
            {
                state = 0;
            }
            break;

        default:
            state = 0;
            break;
    }

    return 0;
}


uint8_t keyK3Pressed(uint32_t now)
{
    static uint8_t state = 0;
    static uint32_t change_time = 0;

    /* PB19低电平表示按下 */
    uint8_t pressed =
        (DL_GPIO_readPins(KEY3_PORT, KEY3_PIN_0_PIN) == 0);

    switch (state)
    {
        case 0:  // 等待按下
            if (pressed)
            {
                change_time = now;
                state = 1;
            }
            break;

        case 1:  // 按下消抖
            if (!pressed)
            {
                state = 0;
            }
            else if ((now - change_time)
                     >= KEY_DEBOUNCE_MS)
            {
                state = 2;
                return 1;
            }
            break;

        case 2:  // 等待松开
            if (!pressed)
            {
                change_time = now;
                state = 3;
            }
            break;

        case 3:  // 松开消抖
            if (pressed)
            {
                state = 2;
            }
            else if ((now - change_time)
                     >= KEY_DEBOUNCE_MS)
            {
                state = 0;
            }
            break;

        default:
            state = 0;
            break;
    }

    return 0;
}