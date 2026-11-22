#include "bsp_uart.h"
#include <stdint.h>

#define UART_TX_WAIT_LIMIT 100000
#define UART_RX_BUFFER_SIZE 128
#define UART_PC_RX_BUFFER_SIZE 128

#if (UART_PC_RX_BUFFER_SIZE & (UART_PC_RX_BUFFER_SIZE - 1)) != 0 || UART_PC_RX_BUFFER_SIZE > 256 || UART_PC_RX_BUFFER_SIZE < 2
#error UART_PC_RX_BUFFER_SIZE must be a power of two and no larger than 256
#endif

static volatile uint8_t s_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;
static volatile uint32_t s_rx_overflow = 0;

/* 两路 PC 各用一个缓冲区，溢出时只清理本路数据。 */
typedef struct
{
    volatile uint8_t buffer[UART_PC_RX_BUFFER_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t lost;
    volatile uint32_t overflow;
    volatile uint32_t overrun;
} UART_PC_ReceiveBuffer;

static UART_PC_ReceiveBuffer s_pc_rx;
static UART_PC_ReceiveBuffer s_pc_alt_rx;

static UART_PC_ReceiveBuffer *UART_PC_GetBuffer(UART_Regs *uart)
{
    if (uart == UART_PC_INST)
    {
        return &s_pc_rx;
    }
    if (uart == UART_PC_ALT_INST)
    {
        return &s_pc_alt_rx;
    }
    return 0;
}



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


uint8_t UART_PC_SendString(const char *str)
{
    uint8_t main_sent = UART_SendString(UART_PC_INST, str);
    uint8_t alt_sent = UART_SendString(UART_PC_ALT_INST, str);
    return main_sent && alt_sent;
}


void UART_StartReceive(void)
{
    NVIC_ClearPendingIRQ(UART_MOTOR_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_MOTOR_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(UART_PC_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_PC_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(UART_PC_ALT_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_PC_ALT_INST_INT_IRQN);
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

int8_t UART_PC_ReadByte(UART_Regs *uart, uint8_t *data)
{
    UART_PC_ReceiveBuffer *rx = UART_PC_GetBuffer(uart);
    if (rx == 0 || data == 0)
    {
        return 0;
    }

    int8_t result = 0;
    uint32_t interrupt_state = __get_PRIMASK();

    /* 短临界区，保证丢失检查、清缓存和取字节不会被接收中断打断。 */
    __disable_irq();
    if (rx->lost)
    {
        rx->tail = rx->head;
        rx->lost = 0;
        result = -1;
    }
    else if (rx->head != rx->tail)
    {
        *data = rx->buffer[rx->tail];
        rx->tail = (rx->tail + 1) & (UART_PC_RX_BUFFER_SIZE - 1);
        result = 1;
    }
    __set_PRIMASK(interrupt_state);

    return result;
}


uint32_t UART_PC_GetOverflowCount(UART_Regs *uart)
{
    UART_PC_ReceiveBuffer *rx = UART_PC_GetBuffer(uart);
    return rx == 0 ? 0 : rx->overflow;
}


uint32_t UART_PC_GetOverrunCount(UART_Regs *uart)
{
    UART_PC_ReceiveBuffer *rx = UART_PC_GetBuffer(uart);
    return rx == 0 ? 0 : rx->overrun;
}


static void UART_PC_ReceiveFIFO(UART_Regs *uart, UART_PC_ReceiveBuffer *rx)
{
    while (!DL_UART_Main_isRXFIFOEmpty(uart))
    {
        uint8_t data = DL_UART_Main_receiveData(uart);

        /* 已丢字节时继续读空硬件 FIFO，等待主循环清缓存和半帧。 */
        if (rx->lost)
        {
            continue;
        }

        uint8_t next = (rx->head + 1) & (UART_PC_RX_BUFFER_SIZE - 1);
        if (next != rx->tail)
        {
            rx->buffer[rx->head] = data;
            rx->head = next;
        }
        else
        {
            rx->overflow++;
            rx->lost = 1;
        }
    }
}


static void UART_PC_HandleInterrupt(UART_Regs *uart, UART_PC_ReceiveBuffer *rx)
{
    DL_UART_IIDX pending;

    /* 一次中断处理完所有已挂起事件，Overrun 不能被当作普通接收。 */
    while ((pending = DL_UART_Main_getPendingInterrupt(uart)) !=
           DL_UART_MAIN_IIDX_NO_INTERRUPT)
    {
        switch (pending)
        {
            case DL_UART_MAIN_IIDX_RX:
            case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
                UART_PC_ReceiveFIFO(uart, rx);
                break;

            case DL_UART_MAIN_IIDX_OVERRUN_ERROR:
                rx->overrun++;
                rx->lost = 1;
                UART_PC_ReceiveFIFO(uart, rx);
                break;

            default:
                break;
        }
    }
}


void UART_PC_INST_IRQHandler(void)
{
    UART_PC_HandleInterrupt(UART_PC_INST, &s_pc_rx);
}


void UART_PC_ALT_INST_IRQHandler(void)
{
    UART_PC_HandleInterrupt(UART_PC_ALT_INST, &s_pc_alt_rx);
}
