#include "telemetry.h"
#include "encoder.h"
#include "pid.h"
#include "line.h"
#include "../BSP/bsp_uart.h"
#include "ti_msp_dl_config.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define TELEMETRY_PERIOD_MS 60

static uint32_t s_last_send_ms = 0;

void telemetry_task(uint32_t now_ms)
{
    if ((now_ms - s_last_send_ms) < TELEMETRY_PERIOD_MS)
    {
        return;
    }

    s_last_send_ms = now_ms;

    /* 速度反馈超过300ms未更新，不发送旧数据 */
    if (!encoder_isFresh(now_ms, 300))
    {
        UART_SendString(UART_PC_INST, "eeeeeeeeeeeee\r\n");
        return;
    }

    char buffer[96];

    int len = snprintf(
        buffer,
        sizeof(buffer),
        "%.1f,%.1f,%.1f,%.1f\r\n",
        encoder_getSpeed(0),
        encoder_getSpeed(1),
        encoder_getSpeed(2),
        encoder_getSpeed(3)
    );

    if (len > 0 && (size_t)len < sizeof(buffer))
    {
        UART_SendString(UART_PC_INST, buffer);
    } 
}




static void PI_ParseCommand(const char *cmd)
{
    if (strncmp(cmd, "PI:", 3) != 0)
    {
        return;
    }

    const char *p = cmd + 3;
    char *end;

    float kp = strtof(p, &end);

    if (end == p || *end != ',')
    {
        return;
    }

    p = end + 1;
    float ki = strtof(p, &end);

    if (end == p || *end != '\0')
    {
        return;
    }

    if (PI_SetGains(kp, ki))
    {
        UART_SendString(UART_PC_INST, "PI:OK\r\n");
    }
}



static void Line_ParseCommand(const char *cmd)
{
    if (strncmp(cmd, "LINEKP:", 7) != 0)
    {
        return;
    }

    const char *p = cmd + 7;
    char *end;

    float kp = strtof(p, &end);

    if (end == p || *end != '\0')
    {
        return;
    }

    if (Line_SetKp(kp))
    {
        UART_SendString(UART_PC_INST, "LINEKP:OK\r\n");
    }
}


void PC_ReceiveTask(void)
{
    static char frame[32];
    static uint8_t length = 0;
    static uint8_t receiving = 0;
    uint8_t data;

    while (UART0_ReadByte(&data))
    {
        if (data == '$')
        {
            length = 0;
            receiving = 1;
            continue;
        }

        if (!receiving)
        {
            continue;
        }

        if (data == '#')
        {
            frame[length] = '\0';

            PI_ParseCommand(frame);
            Line_ParseCommand(frame);

            length = 0;
            receiving = 0;
            continue;
        }
        if (length < sizeof(frame) - 1)
        {
            frame[length++] = (char)data;
        }
        else
        {
            length = 0;
            receiving = 0;
        }
    }
}






void telemetry_debug(uint32_t now_ms)
{
    (void)now_ms;

    uint8_t data;

    /* 将驱动板收到的数据逐字节转发给电脑 */
    while (!DL_UART_isTXFIFOFull(UART_PC_INST) && UART1_ReadByte(&data))
    {
        DL_UART_transmitData(UART_PC_INST, data);
    }
}


void UART_PC_Test(void)
{
    UART_SendString(UART_PC_INST, "Hello MSPM0!\r\n");
    DL_Common_delayCycles(3200000);
}


