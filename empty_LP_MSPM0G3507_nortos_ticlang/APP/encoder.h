
#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

void encoder_init(void);
void encoder_process(uint32_t now_ms);
float encoder_getSpeed(uint8_t motor_id);
uint32_t encoder_getFrameCount(void);
uint8_t encoder_isFresh(uint32_t now_ms, uint32_t max_age_ms);

#endif
