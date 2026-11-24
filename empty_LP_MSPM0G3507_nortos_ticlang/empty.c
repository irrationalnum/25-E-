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



/* 启停处理需要的运行状态，K1 和串口共用。 */
typedef struct
{
    MotorState state;
    uint8_t level;
    uint8_t is_following;
    uint8_t stop_requested;
    int16_t pwm_output[4];
    uint32_t last_control_ms;
    uint32_t last_control_frame;
    uint32_t last_line_ms;
} MotorControl;


static void Motor_SetRunning(uint8_t run, uint32_t now_ms, void *context)
{
    MotorControl *control = context;

    if (!run)
    {
        Line_ResetRecovery();
        PI_MotorsReset(control->pwm_output);
        Car_Stop();
        control->state = MOTOR_STOPPED;
        control->stop_requested = 1;
        return;
    }

    /* 重复 START 不重置积分或丢线计时；同一轮停止优先。 */
    if (control->state == MOTOR_RUNNING || control->stop_requested)
    {
        return;
    }

    Line_ResetRecovery();
    PI_MotorsReset(control->pwm_output);
    Car_Move(speed_table[control->level], 0);
    control->last_control_ms = now_ms;
    control->last_control_frame = encoder_getFrameCount();
    control->last_line_ms = now_ms - LINE_CONTROL_PERIOD_MS;
    control->state = MOTOR_RUNNING;
}


int main(void)
{   
    SYSCFG_DL_init();
    
    encoder_init();
    UART_StartReceive();
    __enable_irq();

    MotorControl control = {0};
    control.state = MOTOR_WAIT;
    control.is_following = 1;

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

        control.stop_requested = 0;

        encoder_process(now);                        //处理驱动板速度汇报
        
        uint8_t k1 = keyK1Pressed(now);
        uint8_t k2 = keyK2Pressed(now);
        uint8_t k3 = keyK3Pressed(now);

        if (k1)                                            //跑停
        {
            Motor_SetRunning(control.state != MOTOR_RUNNING, now, &control);
        }
        else if (k2)                                        //换挡
        {
            if (control.state == MOTOR_RUNNING)
            {
                control.level++;
                if (control.level >= 3)
                {
                    control.level = 0;
                }
                /* 巡线时由下一次检测更新目标，避免停车等待中换挡误启动 */
                if (!control.is_following)
                {
                    Car_Move(speed_table[control.level], 0);
                }
            }
        }
        else if (k3)                                        //循迹
        {
            control.is_following ^= 1;
            Line_ResetRecovery();
            control.last_line_ms = now - LINE_CONTROL_PERIOD_MS;
            if (control.state == MOTOR_RUNNING && !control.is_following)
            {
                Car_Move(speed_table[control.level], 0);
            }
        }


        PC_ReceiveTask(now, Motor_SetRunning, &control);

        /* 巡线调度：运行且巡线开启时，按固定周期更新 */
        if (control.state == MOTOR_RUNNING && control.is_following)
        {
            if ((now - control.last_line_ms) >= LINE_CONTROL_PERIOD_MS)
            {
                control.last_line_ms = now;

                LineFollowResult line_result = Line_Follow(speed_table[control.level], now);
                if (line_result != LINE_FOLLOW_RUNNING)
                {
                    /* 全白等待保留运行请求；全黑或异常组合取消运行 */
                    PI_MotorsReset(control.pwm_output);
                    if (line_result == LINE_FOLLOW_STOPPED)
                        control.state = MOTOR_STOPPED;
                }
            }
        }


        //PI控速
        if (control.state == MOTOR_RUNNING)
        {
            //检查速度反馈是否异常
            uint32_t elapsed_ms = now - control.last_control_ms;
            if (!encoder_isFresh(now, 300) || elapsed_ms > 300)
            {
                Motor_SetRunning(0, now, &control);
                continue;
            }

            //用新的速度反馈调速
            uint32_t frame_count = encoder_getFrameCount();
            if (frame_count != control.last_control_frame)
            {
                float dt = elapsed_ms * 0.001;
                PI_MotorsUpdate(target_speed, dt, control.pwm_output);
                Motor_SetPWM4(control.pwm_output[0], control.pwm_output[1],  control.pwm_output[2], control.pwm_output[3]);


                //记录这次控制更新的信息
                control.last_control_ms = now;
                control.last_control_frame = frame_count;
            }
        }

        if(control.is_following)
        {
            DL_GPIO_togglePins(LED1_PORT, LED1_PIN_3_PIN);
        }

        //telemetry_task(now);
        telemetry_line_debug(now);            //循迹调试
    }
}





// int main(void)
// {
//     SYSCFG_DL_init();

//     while (1) {
//     }
// }
