
#include "pid.h"
#include "motor.h"
#include "encoder.h"


static PI_Controller s_pi_motor[4];

/* 限幅函数 */
static float PI_Limit(float value, float min, float max)
{
    if (value > max)
        return max;

    if (value < min)
        return min;

    return value;
}



void PI_Init(PI_Controller *pi, float kp, float ki)
{
    pi->kp = kp;
    pi->ki = ki;
    pi->integral = 0.0f;
}

void PI_MotorsInit(float kp, float ki)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        PI_Init(&s_pi_motor[i], kp, ki);
    }
}



/* PI计算 */
int16_t PI_Update(PI_Controller *pi, float target_speed, float actual_speed, float dt)
{
    if (target_speed == 0.0f)
    {
        PI_Reset(pi);
        return 0;
    }

    if (dt <= 0.0f)
    {
        return 0;
    }

    /* 1. 计算速度误差 */
    float error = target_speed - actual_speed;

    /* 2. 比例项 */
    float p_out = pi->kp * error;

    /* 3. 积分项 */
    float new_integral = pi->integral + pi->ki * error * dt;

    /* 4. 积分限幅 */
    new_integral = PI_Limit(new_integral, -PI_I_MAX, PI_I_MAX);

    /* 5. 抗积分饱和 */
    float output = p_out + new_integral;

    if (!((output > PI_PWM_MAX && error > 0.0) || (output < -PI_PWM_MAX && error < 0.0)))
    {
        pi->integral = new_integral;
    }

    /* 6. 计算最终输出 */
    output = p_out + pi->integral;

    /* 7. PWM限幅 */
    output = PI_Limit(output, -PI_PWM_MAX, PI_PWM_MAX);

    return (int16_t)output;
}

void PI_MotorsUpdate(const float target_speed[2], float dt, int16_t pwm_out[4])
{
    /* 左轮 */
    pwm_out[LEFT_MOTOR_ID] = PI_Update(&s_pi_motor[LEFT_MOTOR_ID], target_speed[0], encoder_getSpeed(LEFT_MOTOR_ID), dt);

    /* 右轮 */
    pwm_out[RIGHT_MOTOR_ID] = PI_Update(&s_pi_motor[RIGHT_MOTOR_ID], target_speed[1], encoder_getSpeed(RIGHT_MOTOR_ID), dt);
}









/* 清除积分 */
void PI_Reset(PI_Controller *pi)
{
    pi->integral = 0.0f;
}

void PI_MotorsReset(int16_t pwm_out[4])
{
    for (uint8_t i = 0; i < 4; i++)
    {
        PI_Reset(&s_pi_motor[i]);
        pwm_out[i] = 0;
    }
}



uint8_t PI_SetGains(float kp, float ki)
{
    if (!(kp >= 0.0f && kp <= 100.0f && ki >= 0.0f && ki <= 100.0f))
    {
        return 0;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        s_pi_motor[i].kp = kp;
        s_pi_motor[i].ki = ki;
        s_pi_motor[i].integral = 0.0f;
    }

    return 1;
}
