
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

/* 定时向电脑发送电机速度 */
void telemetry_task(uint32_t now_ms);
void PC_ReceiveTask(void);
void telemetry_debug(uint32_t now_ms);
void UART_PC_Test(void);

#endif
