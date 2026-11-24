#include "line.h"
#include "motor.h"
#include "ti_msp_dl_config.h"

/* 已确认传感器输出：白色为高电平，黑色为低电平 */
#define BLACK_LEVEL     0
/* 全白停车等待后连续有效检测次数，调用周期为 20 ms */
#define LINE_RECOVERY_SAMPLES  3
/* 运动中重新捕获需连续两次正常线型，不检查候选偏差变化 */
#define LINE_REACQUIRE_SAMPLES  2
/* 从最近一次可靠实测开始计时，未确认候选不刷新时间 */
#define LINE_LOST_MS         200

typedef enum {
    LINE_NORMAL = 0,
    LINE_GAP,
    LINE_LOST
} LineState;

/* 循迹状态与比例系数 */
static float line_correct_kp = 0.1; /* 默认循迹系数，可通过串口调整 */
static LineState line_state = LINE_NORMAL;

/* 可靠实测历史：全白保持和未确认候选不会更新这些数据 */
static float last_line_offset = 0;
static uint32_t last_reliable_ms = 0;
static uint8_t line_history_valid = 0; /* 居中偏差为 0 也属于有效历史 */
static uint8_t line_candidate_count = 0;

static void Line_Read(uint8_t sensor[4])
{
    uint32_t raw[4]; /* 保留 PA24～PA27 的完整 GPIO 位掩码 */

    raw[0] = DL_GPIO_readPins(LINE_PORT, LINE_S0_PIN);
    raw[1] = DL_GPIO_readPins(LINE_PORT, LINE_S1_PIN);
    raw[2] = DL_GPIO_readPins(LINE_PORT, LINE_S2_PIN);
    raw[3] = DL_GPIO_readPins(LINE_PORT, LINE_S3_PIN);

    /* GPIO 原始读数为低电平 0（黑色）或非零高位掩码（白色）。
     * 转换后的 sensor 按 S0～S3 排列：1 表示检测到黑线，0 表示白色，
     * 是黑线检测标志，不是 GPIO 电平；全黑为 1111，全白为 0000。
     */
    for (uint8_t i = 0; i < 4; i++)
    {
        sensor[i] = (raw[i] == BLACK_LEVEL) ? 1 : 0;
    }
}

static LinePattern Line_Classify(const uint8_t sensor[4], uint8_t *count)
{
    *count = sensor[0] + sensor[1] + sensor[2] + sensor[3];
    if (*count == 0)
        return LINE_WHITE;

    if (*count == 4)
        return LINE_BLACK;

    /* 单根黑线只允许一段连续的黑色检测，不能用非相邻组合更新位置 */
    uint8_t black_groups = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (sensor[i] && (i == 0 || !sensor[i - 1]))
            black_groups++;
    }
    if (black_groups != 1)
        return LINE_INVALID;

    return LINE_VALID;
}

LinePattern Line_GetReading(uint8_t sensor[4], uint8_t *count)
{
    Line_Read(sensor);
    return Line_Classify(sensor, count);
}

static void Line_ResetHistory(void)
{
    last_line_offset = 0;
    last_reliable_ms = 0;
    line_history_valid = 0;
    line_candidate_count = 0;
}

void Line_ResetRecovery(void)
{
    /* 启停/模式切换不继承历史，不改变串口设置的循迹系数 */
    Line_ResetHistory();
    line_state = LINE_NORMAL;
}

static void Line_StateUpdate(LinePattern pattern, uint32_t now)
{
    uint32_t lost_ms = now - last_reliable_ms;
    if ((pattern == LINE_WHITE || line_state == LINE_GAP) &&
        lost_ms >= LINE_LOST_MS)
    {
        /* 200 ms 期限优先于候选确认；本次正常读数算停车后的第 1 次候选 */
        Line_ResetHistory();
        line_state = LINE_LOST;
    }

    if (pattern == LINE_VALID)
    {
        /* 正常巡线直接采用实测值，失检后才需要累计候选 */
        if (line_state == LINE_NORMAL)
            return;

        line_candidate_count++;
        uint8_t required_samples = (line_state == LINE_LOST) ?
                                   LINE_RECOVERY_SAMPLES : LINE_REACQUIRE_SAMPLES;
        if (line_candidate_count >= required_samples)
        {
            line_state = LINE_NORMAL;
            line_candidate_count = 0;
        }
        return;
    }

    /* 到此只剩全白：中断候选确认，无可靠历史时停车 */
    line_candidate_count = 0;
    if (!line_history_valid)
    {
        Line_ResetHistory();
        line_state = LINE_LOST;
        return;
    }

    line_state = LINE_GAP;
}

LineFollowResult Line_Follow(float base_speed, uint32_t now)
{
    uint8_t sensor[4];
    uint8_t count;

    /* 读取实测输入，全黑或异常组合直接取消运行 */
    LinePattern pattern = Line_GetReading(sensor, &count);
    if (pattern == LINE_BLACK || pattern == LINE_INVALID)
    {
        Line_ResetRecovery();
        Car_Stop();
        return LINE_FOLLOW_STOPPED;
    }

    Line_StateUpdate(pattern, now);
    if (line_state == LINE_LOST)
    {
        Car_Stop();
        return LINE_FOLLOW_WAIT;
    }

    /* 间隙保持期间的全白使用最后可靠偏差 */
    float line_offset = last_line_offset;
    if (pattern == LINE_VALID)
    {
        float offset_sum = 3 * sensor[0] + sensor[1] - sensor[2] - 3 * sensor[3];
        line_offset = offset_sum / count;
        /* 未确认候选立即参与转向，只有确认后才更新可靠历史和时间 */
        if (line_state == LINE_NORMAL)
        {
            last_line_offset = line_offset;
            last_reliable_ms = now;
            line_history_valid = 1;
        }
    }

    float turn_ratio = line_correct_kp * line_offset;
    Car_Move(base_speed, base_speed * turn_ratio);
    return LINE_FOLLOW_RUNNING;
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
