#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

/* 定时向电脑发送电机速度 */
void telemetry_task(uint32_t now_ms);

/* run 为 1 请求启动，为 0 请求停车；回调只在主循环中执行。 */
typedef void (*PC_RunHandler)(uint8_t run, uint32_t now_ms, void *context);
void PC_ReceiveTask(uint32_t now_ms, PC_RunHandler run_handler, void *context);
void telemetry_debug(uint32_t now_ms);
void UART_PC_Test(void);

#endif
