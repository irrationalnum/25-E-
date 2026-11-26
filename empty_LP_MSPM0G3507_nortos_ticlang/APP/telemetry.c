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
#define PC_RECEIVE_BYTE_LIMIT 127

typedef struct
{
    char frame[32];
    uint8_t length;
    uint8_t receiving;
    uint8_t run_prefix;
} PC_ReceiveState;

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
        UART_PC_SendString("eeeeeeeeeeeee\r\n");
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
        UART_PC_SendString(buffer);
    } 
}

static void PI_ParseCommand(UART_Regs *uart, const char *cmd)
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
        UART_SendString(uart, "PI:OK\r\n");
    }
}

static void Line_ParseCommand(UART_Regs *uart, const char *cmd)
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
        UART_SendString(uart, "LINEKP:OK\r\n");
    }
}

static void Run_ParseByte(UART_Regs *uart, PC_ReceiveState *state, uint8_t data,
                          uint32_t now_ms, PC_RunHandler run_handler, void *context)
{
    if (state->run_prefix && (data == '2' || data == '3'))
    {
        state->run_prefix = 0;
        if (run_handler != 0)
        {
            run_handler(data == '2', now_ms, context);
            UART_SendString(uart, data == '2' ? "START:OK\r\n" : "STOP:OK\r\n");
        }
        return;
    }

    state->run_prefix = (data == '0');
}

static void PC_ReceivePort(UART_Regs *uart, PC_ReceiveState *state,
                           uint32_t now_ms, PC_RunHandler run_handler, void *context)
{
    uint8_t data;

    /* 每路每轮限量处理，持续输入时也要回到主循环处理另一口和电机。 */
    for (uint8_t count = 0; count < PC_RECEIVE_BYTE_LIMIT; count++)
    {
        int8_t result = UART_PC_ReadByte(uart, &data);
        if (result == 0)
        {
            break;
        }
        if (result < 0)
        {
            /* 只放弃本路半帧；原来在文本帧内时继续丢弃到帧尾。 */
            state->length = 0;
            state->receiving = state->receiving ? 2 : 0;
            state->run_prefix = 0;
            continue;
        }

        if (data == '$')
        {
            state->length = 0;
            state->receiving = 1;
            state->run_prefix = 0;
            continue;
        }
        if (!state->receiving)
        {
            Run_ParseByte(uart, state, data, now_ms, run_handler, context);
            continue;
        }
        if (state->receiving == 2)
        {
            /* 超长或非法文本帧内的 02/03 不能作为启停命令。 */
            if (data == '#')
            {
                state->receiving = 0;
            }
            continue;
        }
        if (data == 0)
        {
            /* ASCII 命令中不允许 NUL，避免截断后误识别为启停命令。 */
            state->length = 0;
            state->receiving = 2;
            continue;
        }
        if (data == '#')
        {
            state->frame[state->length] = '\0';
            PI_ParseCommand(uart, state->frame);
            Line_ParseCommand(uart, state->frame);
            state->length = 0;
            state->receiving = 0;
            continue;
        }
        if (state->length < sizeof(state->frame) - 1)
        {
            state->frame[state->length++] = (char)data;
        }
        else
        {
            state->length = 0;
            state->receiving = 2;
        }
    }
}

void PC_ReceiveTask(uint32_t now_ms, PC_RunHandler run_handler, void *context)
{
    static PC_ReceiveState main_state;
    static PC_ReceiveState alt_state;

    PC_ReceivePort(UART_PC_INST, &main_state, now_ms, run_handler, context);
    PC_ReceivePort(UART_PC_ALT_INST, &alt_state, now_ms, run_handler, context);
}

void telemetry_debug(uint32_t now_ms)
{
    (void)now_ms;

    uint8_t data;

    /* 将驱动板收到的数据逐字节转发给电脑 */
    while (!DL_UART_isTXFIFOFull(UART_PC_INST) &&
           !DL_UART_isTXFIFOFull(UART_PC_ALT_INST) && UART1_ReadByte(&data))
    {
        DL_UART_transmitData(UART_PC_INST, data);
        DL_UART_transmitData(UART_PC_ALT_INST, data);
    }
}

void UART_PC_Test(void)
{
    UART_PC_SendString("Hello MSPM0!\r\n");
    DL_Common_delayCycles(3200000);
}
