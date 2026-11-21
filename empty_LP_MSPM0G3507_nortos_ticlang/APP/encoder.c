
#include "encoder.h"
#include "../BSP/bsp_uart.h"

#include <stdlib.h>
#include <string.h>


/* 接收帧缓冲区 */
static char s_frame[64];
static uint8_t s_length = 0;
static uint8_t s_receiving = 0;

/* 四个电机的实际速度，单位mm/s */
static float s_speed[4] = {0};

/* 解析成功的速度帧数量 */
static uint32_t s_frame_count = 0;

/* 最近一次有效速度数据的时间 */
static uint32_t s_last_update_ms = 0;

/* 是否已接收到至少一帧有效数据 */
static uint8_t s_has_data = 0;




static void encoder_parseFrame(const char *frame, uint32_t now_ms)
{
    float values[4];
    const char *p = frame;

    if (strncmp(p, "$MSPD:", 6) != 0)
    {
        return;
    }
    p += 6;

    for (uint8_t i = 0; i < 4; i++)
    {
        char *end;

        values[i] = strtof(p, &end);

        if (end == p)
        {
            return;
        }
        if (!(values[i] >= -100000 && values[i] <= 100000))
        {
            return;
        }

        if (i < 4 - 1)
        {
            if (*end != ',')
            {
                return;
            }

            p = end + 1;
        }
        else
        {
            if (*end != '\0')
            {
                return;
            }
        }
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        s_speed[i] = values[i];
    }

    s_frame_count++;
    s_last_update_ms = now_ms;
    s_has_data = 1;
}

void encoder_init(void)
{
    s_length = 0;
    s_receiving = 0;
    s_frame_count = 0;
    s_last_update_ms = 0;
    s_has_data = 0;

    for (uint8_t i = 0; i < 4; i++)
    {
        s_speed[i] = 0;
    }
}

void encoder_process(uint32_t now_ms)
{
    uint8_t data;

    while (UART1_ReadByte(&data))
    {
        if (data == '$')
        {
            s_length = 0;
            s_receiving = 1;
            s_frame[s_length++] = '$';
            continue;
        }

        /* 没有帧头时忽略数据 */
        if (!s_receiving)
        {
            continue;
        }

        if (data == '#')
        {
            s_frame[s_length] = '\0';

            encoder_parseFrame(s_frame, now_ms);

            s_length = 0;
            s_receiving = 0;
            continue;
        }

        /* 保存普通数据字节 */
        if (s_length < 63)
        {
            s_frame[s_length++] = (char)data;
        }
        else
        {
            /* 数据超长，丢弃当前帧 */
            s_length = 0;
            s_receiving = 0;
        }
    }
}

/* motor_id: 0~3 */
float encoder_getSpeed(uint8_t motor_id)
{
    if (motor_id > 3)
    {
        return 0;
    }

    return s_speed[motor_id];
}

uint32_t encoder_getFrameCount(void)
{
    return s_frame_count;
}

/* 是否具有新鲜的速度反馈 */
uint8_t encoder_isFresh(uint32_t now_ms, uint32_t max_age_ms)
{
    if (!s_has_data)
    {
        return 0;
    }

    return (now_ms - s_last_update_ms) <= max_age_ms;
}
