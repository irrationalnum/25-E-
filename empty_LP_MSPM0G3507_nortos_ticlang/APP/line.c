
#include "line.h"
#include "motor.h"
#include "ti_msp_dl_config.h"

/* 红外模块检测黑线时的电平 */
#define BLACK_LEVEL     0
/* 最大转向修正比例 */
#define LINE_MAX_TURN_RATIO  0.3
/* 丢线/全黑后连续有效检测次数，调用周期为 20 ms */
#define LINE_RECOVERY_SAMPLES  3U




static float line_correct_kp = 0.1f; /* 默认循迹系数，可通过串口调整 */
static uint8_t line_valid_samples = LINE_RECOVERY_SAMPLES;

static void Line_Read(uint8_t sensor[4])
{
    uint32_t raw[4]; /* 保留 PA24～PA27 的完整 GPIO 位掩码 */

    raw[0] = DL_GPIO_readPins(LINE_PORT, LINE_S0_PIN);
    raw[1] = DL_GPIO_readPins(LINE_PORT, LINE_S1_PIN);
    raw[2] = DL_GPIO_readPins(LINE_PORT, LINE_S2_PIN);
    raw[3] = DL_GPIO_readPins(LINE_PORT, LINE_S3_PIN);

    for (uint8_t i = 0; i < 4; i++)
    {
        sensor[i] = (raw[i] == BLACK_LEVEL) ? 1 : 0;
    }
}


void Line_ResetRecovery(void)
{
    /* 新的启动/模式切换不继承上一次的恢复计数 */
    line_valid_samples = LINE_RECOVERY_SAMPLES;
}


uint8_t Line_Follow(float base_speed)
{
    uint8_t sensor[4];

    Line_Read(sensor);

    uint8_t count = sensor[0] + sensor[1] + sensor[2] + sensor[3];

    // 丢线或者全部检测到黑色：停车并重新等待稳定线型
    if (count == 0 || count == 4)
    {
        line_valid_samples = 0;
        Car_Stop();
        return 0;
    }

    if (line_valid_samples < LINE_RECOVERY_SAMPLES)
    {
        line_valid_samples++;
        if (line_valid_samples < LINE_RECOVERY_SAMPLES)
        {
            Car_Stop();
            return 0;
        }
    }


    float line_offset = (3.0 * sensor[0] + sensor[1] - sensor[2] - 3.0 * sensor[3]) / count;

    float turn = base_speed * line_correct_kp * line_offset;

    float max_turn = base_speed * LINE_MAX_TURN_RATIO;
    if (turn > max_turn)
    {
        turn = max_turn;
    }
    else if (turn < -max_turn)
    {
        turn = -max_turn;
    }

    Car_Move(base_speed, turn);

    return 1;
}


uint8_t Line_SetKp(float kp)
{
    if (!(kp >= 0 && kp <= 1))
    {
        return 0;
    }

    line_correct_kp = kp;
    return 1;
}