#include "motor.h"
#include "../BSP/bsp_uart.h"

#include <stdio.h>



float target_speed[2] = {0};



static int16_t Motor_LimitPWM(int16_t pwm)
{
    if (pwm > 3600)
    {
        return 3600;
    }

    if (pwm < -3600)
    {
        return -3600;
    }

    return pwm;
}



void Motor_SetPWM4(int16_t pwm1, int16_t pwm2, int16_t pwm3, int16_t pwm4)
{
    char cmd[48];

    snprintf(cmd, sizeof(cmd),  "$pwm:%d,%d,%d,%d#", Motor_LimitPWM(pwm1), Motor_LimitPWM(pwm2), Motor_LimitPWM(pwm3), Motor_LimitPWM(pwm4));

    UART_SendString(UART_MOTOR_INST, cmd);
}

void Car_SetSpeed(float left_speed, float right_speed)
{
    target_speed[0] = left_speed;
    target_speed[1] = right_speed;
}

void Car_Move(float base_speed, float turn)
{
    Car_SetSpeed(base_speed - turn, base_speed + turn);
}

void Car_Stop(void)
{
    Car_SetSpeed(0, 0);
    UART_SendString(UART_MOTOR_INST, "$pwm:0,0,0,0#");
}



void Motor_Config310(void)
{
    UART_SendString(UART_MOTOR_INST, "$mtype:2#");
    DL_Common_delayCycles(3200000);

    UART_SendString(UART_MOTOR_INST, "$mphase:20#");
    DL_Common_delayCycles(3200000);

    UART_SendString(UART_MOTOR_INST, "$mline:13#");
    DL_Common_delayCycles(3200000);

    UART_SendString(UART_MOTOR_INST, "$wdiameter:48#");
    DL_Common_delayCycles(3200000);

    UART_SendString(UART_MOTOR_INST, "$deadzone:1600#");
    DL_Common_delayCycles(3200000);
}