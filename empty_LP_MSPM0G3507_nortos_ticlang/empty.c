/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"

#include "BSP/bsp_systick.h"
#include "BSP/bsp_key.h"
#include "BSP/bsp_uart.h"
#include "APP/motor.h"
#include "APP/encoder.h"
#include "APP/pid.h"
#include "APP/line.h"
#include "APP/telemetry.h"
#include <stdio.h>




typedef enum{
    MOTOR_WAIT = 0,
    MOTOR_RUNNING,
    MOTOR_STOPPED
}MotorState;

static const float speed_table[3] =
{
    150,
    300,
    600
};



int main(void)
{   
    SYSCFG_DL_init();
    
    encoder_init();
    UART_StartReceive();
    __enable_irq();

    uint8_t level = 0;                    //速度档位
    MotorState state = MOTOR_WAIT;        //初始化状态

    int16_t pwm_output[4] = {0};          //PI计算得到的PWM值，通过Motor_SetPWM4传给驱动板
    uint32_t last_control_ms = 0;         //上次控制时刻
    uint32_t last_control_frame = 0;      //上个速度反馈帧
    uint32_t last_line_ms = 0;            //上次循迹控制时刻
    uint8_t is_following = 1;             //循迹开关

    PI_MotorsInit(1.5, 8);           //PID
    Car_Stop();
    DL_Common_delayCycles(320000);
    UART_SendString(UART_MOTOR_INST, "$upload:0,0,1#");         //驱动板速度汇报
    DL_Common_delayCycles(320000);
    UART_PC_SendString("...start...\r\n");
    DL_Common_delayCycles(320000);

    while (1)
    {
        uint32_t now = get_millis();

        encoder_process(now);                        //处理驱动板速度汇报
        
        uint8_t k1 = keyK1Pressed(now);
        uint8_t k2 = keyK2Pressed(now);
        uint8_t k3 = keyK3Pressed(now);

        if (k1)                                            //跑停
        {
            switch (state)
            {
                case MOTOR_WAIT:
                case MOTOR_STOPPED:
                    Line_ResetRecovery();
                    PI_MotorsReset(pwm_output);
                    Car_Move(speed_table[level], 0);

                    last_control_ms = now;
                    last_control_frame = encoder_getFrameCount();
                    last_line_ms = now - LINE_CONTROL_PERIOD_MS;
                    
                    state = MOTOR_RUNNING;
                    break;

                case MOTOR_RUNNING:
                    Line_ResetRecovery();
                    PI_MotorsReset(pwm_output);
                    Car_Stop();
                    
                    state = MOTOR_STOPPED;
                    break;

                default:
                    Line_ResetRecovery();
                    Car_Stop();
                    state = MOTOR_STOPPED;
                    break;
            }
        }
        else if (k2)                                        //换挡
        {
            if (state == MOTOR_RUNNING)
            {
                level++;
                if (level >= 3)
                {
                    level = 0;
                }
                /* 巡线时由下一次检测更新目标，避免停车等待中换挡误启动 */
                if (!is_following)
                {
                    Car_Move(speed_table[level], 0);
                }
            }
        }
        else if (k3)                                        //循迹
        {
            is_following ^= 1;
            Line_ResetRecovery();
            last_line_ms = now - LINE_CONTROL_PERIOD_MS;
            if (state == MOTOR_RUNNING && !is_following)
            {
                Car_Move(speed_table[level], 0);
            }
        }


        /* 巡线调度：运行且巡线开启时，按固定周期更新 */
        if (state == MOTOR_RUNNING && is_following)
        {
            if ((now - last_line_ms) >= LINE_CONTROL_PERIOD_MS)
            {
                last_line_ms = now;

                LineFollowResult line_result = Line_Follow(speed_table[level], now);
                if (line_result != LINE_FOLLOW_RUNNING)
                {
                    /* 全白等待保留运行请求；全黑或异常组合取消运行 */
                    PI_MotorsReset(pwm_output);
                    if (line_result == LINE_FOLLOW_STOPPED)
                        state = MOTOR_STOPPED;
                }
            }
        }


        //PI控速
        if (state == MOTOR_RUNNING)
        {
            //检查速度反馈是否异常
            uint32_t elapsed_ms = now - last_control_ms;
            if (!encoder_isFresh(now, 300) || elapsed_ms > 300)
            {
                Line_ResetRecovery();
                Car_Stop();
                PI_MotorsReset(pwm_output);

                state = MOTOR_STOPPED;
                continue;
            }

            //用新的速度反馈调速
            uint32_t frame_count = encoder_getFrameCount();
            if (frame_count != last_control_frame)
            {
                float dt = elapsed_ms * 0.001;
                PI_MotorsUpdate(target_speed, dt, pwm_output);
                Motor_SetPWM4(pwm_output[0], pwm_output[1],  pwm_output[2], pwm_output[3]);


                //记录这次控制更新的信息
                last_control_ms = now;
                last_control_frame = frame_count;
            }
        }

        if(is_following)
        {
            DL_GPIO_togglePins(LED1_PORT, LED1_PIN_2_PIN);
        }

        telemetry_task(now);
        PC_ReceiveTask();
    }
}





// int main(void)
// {
//     SYSCFG_DL_init();

//     while (1) {
//     }
// }
