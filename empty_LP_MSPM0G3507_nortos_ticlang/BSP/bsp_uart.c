#include "bsp_uart.h"
#include <stdint.h>

#define UART_TX_WAIT_LIMIT 100000
#define UART_RX_BUFFER_SIZE 128

static volatile uint8_t s_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;
static volatile uint32_t s_rx_overflow = 0;



uint8_t UART_SendString(UART_Regs *uart, const char *str)
{
    if (uart == 0 || str == 0)
    {
        return 0;
    }

    while (*str != '\0')
    {
        uint32_t wait_count = UART_TX_WAIT_LIMIT;

        /* 只有发送 FIFO 满了，才需要等待 */
        while (DL_UART_isTXFIFOFull(uart))
        {
            if (wait_count == 0)
            {
                /* 等待超过上限，退出，不再无限等待 */
                return 0;
            }

            wait_count--;
        }

        /* FIFO 有空位，写入一个字节 */
        DL_UART_transmitData(uart, (uint8_t)*str);

        str++;
    }

    return 1;
}


void UART_StartReceive(void)
{
    NVIC_ClearPendingIRQ(UART_MOTOR_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_MOTOR_INST_INT_IRQN);
}


uint8_t UART1_ReadByte(uint8_t *data)
{
    if (data == 0)
    {
        return 0;
    }

    /* 缓冲区为空 */
    if (s_rx_head == s_rx_tail)
    {
        return 0;
    }

    *data = s_rx_buffer[s_rx_tail];

    /* 读取位置向后移动 */
    s_rx_tail = (s_rx_tail + 1) & (UART_RX_BUFFER_SIZE - 1);

    return 1;
}


uint32_t UART_GetOverflowCount(void)
{
    return s_rx_overflow;
}


void UART_MOTOR_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(
                UART_MOTOR_INST))
    {
        case DL_UART_MAIN_IIDX_RX:
        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:

            /* 读完FIFO中已经收到的字节 */
            while (!DL_UART_Main_isRXFIFOEmpty(UART_MOTOR_INST))
            {
                uint8_t data =
                    DL_UART_Main_receiveData(UART_MOTOR_INST);

                uint8_t next = (s_rx_head + 1) & (UART_RX_BUFFER_SIZE - 1);

                /* 缓冲区未满 */
                if (next != s_rx_tail)
                {
                    s_rx_buffer[s_rx_head] = data;
                    s_rx_head = next;
                }
                else
                {
                    /* 满了就丢弃新字节并记录 */
                    s_rx_overflow++;
                }
            }
            break;

        default:
            break;
    }
}

uint8_t UART0_ReadByte(uint8_t *data)
{
    if (data == 0 || DL_UART_Main_isRXFIFOEmpty(UART_PC_INST))
    {
        return 0;
    }

    *data = DL_UART_Main_receiveData(UART_PC_INST);
    return 1;
}


