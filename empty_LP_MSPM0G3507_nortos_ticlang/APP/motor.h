#ifndef MOTOR_H
#define MOTOR_H

#include "ti_msp_dl_config.h"


#define LEFT_MOTOR_ID     1
#define RIGHT_MOTOR_ID    0

extern float target_speed[2];



void Motor_Config310(void);
void Motor_SetPWM4(int16_t pwm1, int16_t pwm2, int16_t pwm3, int16_t pwm4);

void Car_SetSpeed(float left_speed, float right_speed);
void Car_Move(float base_speed, float turn);
void Car_Stop(void);

#endif
