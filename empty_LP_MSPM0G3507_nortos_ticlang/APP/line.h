#ifndef LINE_H
#define LINE_H

#include <stdint.h>

/* 本次实测线型 */
typedef enum {
    LINE_VALID = 0,
    LINE_WHITE,
    LINE_BLACK,
    LINE_INVALID
} LinePattern;

/* 主循环调度周期，也是连续调用的最小间隔，单位 ms */
#define LINE_CONTROL_PERIOD_MS  20

typedef enum {
    LINE_FOLLOW_WAIT = 0,    /* 全白停车等待，可在确认找回后恢复 */
    LINE_FOLLOW_RUNNING = 1,
    LINE_FOLLOW_STOPPED = 2  /* 全黑或异常停止，须按 K1 重新启动 */
} LineFollowResult;

/* 只读取并分类当前四路输入，不改变循迹历史、候选或电机目标。 */
LinePattern Line_GetReading(uint8_t sensor[4], uint8_t *count);

/* 清除循迹历史和候选计数，保留当前比例系数 */
void Line_ResetRecovery(void);

/* 每 LINE_CONTROL_PERIOD_MS ms 调用，同次运行中相邻调用至少间隔该周期；
 * now 为实际毫秒数，用于丢线期限，返回值见 LineFollowResult。
 * 全白有可靠历史时保持该偏差（为 0 则直行），无历史立即等待。
 * 从最近可靠检测计时，不足 200 ms 为间隙保持，达到 200 ms 后等待；全黑或非相邻组合要求 MOTOR_STOPPED。
 * 运动中连续 2 次正常检测确认找回，全白停车等待后连续 3 次恢复；不检查候选偏差变化。
 * 首次运动候选立即修正但不刷新历史；转向直接按比例计算，不作限幅或平滑。
 */
LineFollowResult Line_Follow(float base_speed, uint32_t now);

/* 设置 0～1 的比例系数；较大系数允许反向轮速目标；成功返回 1，非法参数返回 0 */
uint8_t Line_SetKp(float kp);

#endif
