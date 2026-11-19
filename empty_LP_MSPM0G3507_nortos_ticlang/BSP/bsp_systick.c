#include "bsp_systick.h"

/* 仅本文件使用的毫秒计数变量 */
static volatile uint32_t s_ms = 0;

/* SysTick中断函数 */
void SysTick_Handler(void)
{
    s_ms++;
}

/* 获取当前毫秒计数 */
uint32_t get_millis(void)
{
    return s_ms;
}