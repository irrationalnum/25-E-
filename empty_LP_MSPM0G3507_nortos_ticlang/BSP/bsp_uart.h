#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include "ti_msp_dl_config.h"

uint8_t UART_SendString(UART_Regs *uart, const char *str);
void UART_StartReceive(void);
uint8_t UART1_ReadByte(uint8_t *data);
uint8_t UART0_ReadByte(uint8_t *data);
uint32_t UART_GetOverflowCount(void);


#endif