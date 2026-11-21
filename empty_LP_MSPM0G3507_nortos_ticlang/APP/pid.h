
#ifndef PID_H
#define PID_H

#include <stdint.h>

/* PWM输出和积分限幅 */
#define PI_PWM_MAX  3600
#define PI_I_MAX    1800

typedef struct
{
    float kp;        
    float ki;         
    float integral;    
} PI_Controller;

void PI_Init(PI_Controller *pi, float kp, float ki);
void PI_Reset(PI_Controller *pi);
int16_t PI_Update(PI_Controller *pi, float target_speed, float actual_speed, float dt);
void PI_MotorsInit(float kp, float ki);
void PI_MotorsReset(int16_t pwm_out[4]);
void PI_MotorsUpdate(const float target_speed[2], float dt, int16_t pwm_out[4]);
uint8_t PI_SetGains(float kp, float ki);

#endif
