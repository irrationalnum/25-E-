#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include "ti_msp_dl_config.h"

uint8_t UART_SendString(UART_Regs *uart, const char *str);
/* 向两个 PC 串口发送，任一路失败返回 0，但仍尝试另一路。 */
uint8_t UART_PC_SendString(const char *str);
void UART_StartReceive(void);
uint8_t UART1_ReadByte(uint8_t *data);
/* 返回 1 表示读到字节，0 表示无数据，-1 表示接收丢失并已清空缓存。 */
int8_t UART_PC_ReadByte(UART_Regs *uart, uint8_t *data);
uint32_t UART_GetOverflowCount(void);
/* PC 软件缓冲区满和硬件 Overrun 分别计数，不在串口输出诊断文字。 */
uint32_t UART_PC_GetOverflowCount(UART_Regs *uart);
uint32_t UART_PC_GetOverrunCount(UART_Regs *uart);


#endif