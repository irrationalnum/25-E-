#ifndef BSP_KEY_H
#define BSP_KEY_H

#include <stdint.h>

/* 检测到一次有效K1按下，返回1 */
uint8_t keyK1Pressed(uint32_t now);
uint8_t keyK2Pressed(uint32_t now);
uint8_t keyK3Pressed(uint32_t now);


#endif