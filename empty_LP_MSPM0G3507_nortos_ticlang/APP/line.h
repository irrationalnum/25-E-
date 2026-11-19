#ifndef LINE_H
#define LINE_H

#include <stdint.h>

void Line_ResetRecovery(void);
/* 每 20 ms 调用；返回 0 表示停车等待，丢线/全黑后连续 3 次有效检测才恢复 */
uint8_t Line_Follow(float base_speed);
uint8_t Line_SetKp(float kp);

#endif