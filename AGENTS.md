<!-- DO NOT EDIT - This part is automatically generated. -->

# Agent Guidelines

## CCStudio IDE Installation Directory

CCStudio IDE is installed at `E:/Victory/application/productivity/CCS/CCS_21`. Save it as `{ccs-install-dir}` for the session — scripts and tools will need it.

## MANDATORY Pre-Task Requirement (DO NOT SKIP)

**CRITICAL - NO EXCEPTIONS**: Before ANY CCS/Texas Instruments-related task (even simple ones), you MUST read `E:/Victory/application/productivity/CCS/CCS_21/ccs/theia/resources/ai/CCS.md`. This file includes information on how to interact with CCS as well as device-specific information (UART backchannel pins, LED setup, transmit best practices, etc.). 

Do NOT call any ccs-project, ccs-debug, ccs-sysconfig, or ccs-serial MCP tools until CCS.md has been read.


<!-- DO NOT EDIT - This part is automatically generated. -->

<!-- User instructions should be added below this line -->

## 项目开发约定

本节适用于当前工作区。保留上方自动生成内容，只在用户指令区维护项目约定。默认使用中文交流，说明修改内容和实际验证结果；不要把编译通过表述为实车验证通过。

### 项目定位与资料入口

- 当前工程：`empty_LP_MSPM0G3507_nortos_ticlang/`。以下源码路径均相对此目录。
- 用户已确认小车为**两轮差速驱动**。MCU 为 MSPM0G3507，使用 DriverLib、裸机主循环和 1 ms SysTick，无 RTOS。
- 外部驱动板负责输出电机 PWM、采集编码器并通过串口上报速度；MCU 不直接输出电机 PWM，也不直接计数编码器脉冲。
- 先阅读工程内的 `README.md` 了解接线、运行和协议；`README.html` 是原 SDK 空工程说明。历史 ZIP 仅作备份，不作为当前源码或构建入口。
- 现状以源码及有效工程配置核对，需求以用户当前指令为准。修改默认参数、协议或接线时同步更新相关文档，避免将计划中的功能写成已实现。

### 环境与 CCS 工具使用

当前环境基线如下；操作前用 CCS Project MCP 核对依赖和路径，不假设其他机器安装位置相同。

| 项目 | 当前配置 |
| --- | --- |
| CCS | `E:/Victory/application/productivity/CCS/CCS_21` |
| MSPM0 SDK | `2.1.0.03`，`E:/Victory/work/a1/TI/SDK/mspm0_sdk_2_01_00_03` |
| TI Arm Clang | `5.1.1.LTS` |
| 工程 SysConfig | `1.21.0`，`E:/Victory/work/a1/TI/ti/sysconfig_1.21.0` |
| 构建配置 | `Debug`，`-O2` |
| CPU 时钟 | 32 MHz，SysTick 为 1 ms |
| 活动调试配置 | `targetConfigs/MSPM0G3507.ccxml`，XDS110 |

- 已读取上方要求的 `CCS.md` 后，使用 Project MCP 查询工程设置和产品依赖；不要直接修改 `.project`、`.ccsproject`、`.cproject` 或 `.settings/`。
- SDK 专属指南先检查 `{SDK目录}/AGENTS.md`；若不存在，再检查 `{ccs-install-dir}/ccs/theia/resources/ai/sdks/MSPM0-SDK/AGENTS.md`。板卡指南位于 `{ccs-install-dir}/ccs/theia/resources/ai/boards/LP-MSPM0G3507/AGENTS.md`。配置外设前阅读相关指南。
- SDK 内容只读。不要修改 SDK 驱动、启动文件或示例来绕过本工程的问题。
- `.syscfg` 只能通过 SysConfig MCP 读取和修改；不要手工编辑，也不要修改 `Debug/ti_msp_dl_config.c/.h` 等生成文件作为修复手段。
- 当前 SysConfig MCP 使用 `1.28.1`，打开工程的 `1.21.0` 配置会失败。这不妨碍已有构建工具编译工程。先记录版本不匹配；只有任务确需修改配置时才处理工具版本，不要为查看配置自动迁移用户工程。更改工程 SysConfig 版本前须取得用户确认。
- 配置暂时无法通过 MCP 打开时，可只读已有生成文件核对引脚与外设参数，注明它们只是最近一次生成的结果；不能据此绕过 SysConfig 修改配置。
- 不要因目录同时存在 `MSPM0G3519.ccxml` 就切换器件。若发现工程、封装、调试配置或实物信息不一致，先核实，不静默迁移。

### 代码职责与修改入口

| 位置 | 职责 |
| --- | --- |
| `empty.c` | 初始化、运行状态、按键操作、巡线及速度闭环调度 |
| `APP/motor.c/.h` | 左右目标速度、驱动板 PWM 指令、停车和通道映射 |
| `APP/pid.c/.h` | 速度 PI、积分与输出限幅、运行时参数设置 |
| `APP/encoder.c/.h` | 驱动板四路速度帧解析、反馈时效及帧计数 |
| `APP/line.c/.h` | 四路巡线输入、可靠偏差保持、丢线状态机、差速修正及巡线系数 |
| `APP/telemetry.c/.h` | PC 遥测、PI 和巡线参数命令解析 |
| `BSP/` | UART、按键消抖、毫秒计时 |
| `empty.syscfg` | 时钟、SysTick、GPIO 和 UART 配置源 |

沿用现有 C 代码、DriverLib API、模块命名及缩进风格。修改尽量局限于需求相关模块，不顺带引入 RTOS、重构控制架构或更换电机协议。

### 代码风格

- 手写代码采用简单的学生风格，普通数字常量不加 `U`、`f` 后缀，例如 `200`、`0.1`；整数值直接写整数。
- 不加冗余的类型转换；仅保留必要类型和转换。

### 控制逻辑与必须保留的行为

- 驱动板报文始终包含四通道，数组索引为 `0～3`；当前左轮 `LEFT_MOTOR_ID = 1`、右轮 `RIGHT_MOTOR_ID = 3`，对应报文第 **2 / 4** 项。不要按“两轮”把协议数组改为两项。正常闭环只更新这两路，其余通道输出保持为 0。
- `target_speed[0]`、`target_speed[1]` 分别表示左右目标速度，单位 mm/s；`Car_Move(base_speed, turn)` 设置左轮为 `base_speed - turn`、右轮为 `base_speed + turn`。
- 档位为 150 / 300 / 600 mm/s，默认第一档。速度 PI 默认 `Kp = 1.5`、`Ki = 8.0`，没有微分项；PWM 限幅 ±3600，积分项限幅 ±1800，保留抗积分饱和处理。
- 速度 PI 仅在收到新有效反馈帧时更新，`dt` 用实际毫秒差换算成秒。保留反馈未收到、反馈超过 300 ms 未更新以及控制更新间隔超过 300 ms 时的停车逻辑；调整超时阈值时同时检查主循环与遥测模块。
- `Car_Stop()` 同时清零目标速度并向驱动板发送零 PWM。保留启停时的 PI 积分清零，避免旧积分影响重新启动。
- 巡线默认开启，20 ms 更新一次；低电平代表黑线，S0～S3 权重依次为 `+3、+1、-1、-3`。巡线系数默认 `0.1`，允许范围 `0～1`，直接按 `turn = base_speed * Kp * line_offset` 计算转向，不加转向限幅或平滑；较大 Kp 允许内侧轮目标反转。基础速度保持当前档位，不加入 PD 或动态调速，保留电机 PWM 和 PI 积分限幅。
- 赛道只有一根黑线，正常线型必须是连续相邻的 1～3 路黑线；非相邻组合 `0101、1001、1010、1011、1101` 是异常检测，与全黑一样立即发送零 PWM、清除历史和候选，由主循环清零 PI 并进入 MOTOR_STOPPED；线型恢复和 K2/K3 都不能重新起步，须按 K1 启动。GPIO 位宽、权重及正常偏差计算保持不变。
- 循迹为正常、间隙保持、限时寻线、停车等待四状态。全白且有可靠历史时，从最近可靠检测开始计时：不足 80 ms 为间隙，达到 80 ms 且不足 200 ms 为寻线，两阶段均保持最后可靠偏差，偏差为 0 则按当前档位直行，首次达到或超过 200 ms 的循迹更新优先停车；无可靠历史时全白立即停车。可靠历史有效性用独立标志记录，偏差或时间戳为 0 也有效；清除历史时同时清除此标志。运动中连续 2 次正常线型确认找回，全白停车等待后连续 3 次正常线型恢复；候选位置跳变不重计数，全白/全黑/异常组合清零候选。首次运动候选立即用于转向但不更新可靠历史和时间，确认后以当前实测值重建历史；到 200 ms 仍未确认时先停车，当次正常读数只能算停车后的第 1 次候选。停车清除 PI 积分及可靠历史，前两次候选保持零目标和零 PWM。等待期间仍检查反馈，反馈保护停车或 K1 手动停止后不得自动恢复。
- 全白不进行趋势外推、不强制使用最大偏差寻线，统一使用最后可靠偏差；正常和候选转向直接响应，候选后再次全白也直接恢复最后可靠偏差的转向。系数减小或设为零时下一次循迹更新立即按新值计算。启停、反馈保护和模式切换清除历史及候选；未确认候选不能刷新可靠时间或污染实测历史。`Line_Follow()` 同次运行中相邻调用至少间隔 20 ms，时间窗口使用实际毫秒差，不能用采样次数延长。
- K1 启停（含取消巡线停车等待），K2 仅在运行时循环换挡（停车等待中只更新档位，不能输出行驶目标），K3 切换巡线；关闭巡线后恢复当前档位的直行目标。启停、反馈保护及切换巡线时重置历史和恢复计数，但保留串口设置的巡线系数。按键使用 20 ms 非阻塞消抖。
- UART1 中断只负责搬运字节到环形缓冲区，帧解析和控制运算放在主循环。保留发送等待上限、接收边界检查及溢出计数；依赖位掩码回绕的缓冲区大小须为 2 的幂。
- 调度时间差沿用无符号减法，兼容毫秒计数回绕。不要在持续运行的控制循环或中断中增加长时间阻塞延时。

### 引脚与串口协议约定

以下引脚依据现有生成配置核对；改变接线前需重新核对配置和实物。

| 功能 | 当前引脚 / 参数 |
| --- | --- |
| 电机驱动板 `UART_MOTOR` | UART1，TX = PB6，RX = PB7 |
| 电脑 `UART_PC` | UART0，TX = PA10，RX = PA11，XDS110 回传串口 |
| 两路 UART 格式 | 115200 baud，8N1，无硬件流控 |
| 按键 K1 / K2 / K3 | PA2 / PB19 / PB20，低电平有效 |
| 巡线 S0 / S1 / S2 / S3 | PA24 / PA25 / PA26 / PA27 |
| 状态输出实例 `LED1` | PB2，不是 LaunchPad 板载 LED1 的 PA0 |

- 工程的 K1/K2/K3 是外接按键，不能按 LaunchPad 自带 S1/S2 引脚替换。`bsp_key.c` 中部分引脚注释可能陈旧，应以实际使用的配置为准。
- 使用 XDS110 回传串口时核对 J21/J22；外接巡线输入 PA26/PA27 时核对板载光传感器的 J18/J17，详细接线见 README 和板卡指南。
- 电机输出为 `$pwm:p0,p1,p2,p3#`，反馈为 `$MSPD:v0,v1,v2,v3#`。仅完整合法反馈帧刷新速度和时效；保留对坏帧、超长帧及重同步的处理。
- 启动时发送 `$upload:0,0,1#` 请求速度上报。`Motor_Config310()` 当前未在主程序调用，不要假设驱动板上电即已完成电机参数配置，也不要无关地加入调用。
- PC 调参命令为 `$PI:1.5,8.0#` 和 `$LINEKP:0.1#`，以 `$` / `#` 定界，无需换行。PI 两参数范围均为 `0～100`，巡线系数为 `0～1`；成功响应分别为 `PI:OK\r\n`、`LINEKP:OK\r\n`。
- PC 参数只存 RAM，复位恢复源码默认值。不要在未要求时增加 Flash 持久化或改变现有命令、响应格式。
- 遥测每 60 ms 发送四路速度 CSV，保留一位小数和 `\r\n` 行尾；反馈异常时发送 `eeeeeeeeeeeee\r\n`。调参响应与遥测共用串口，修改时考虑上位机兼容。
- `telemetry_debug()` 和 `encoder_process()` 都会消费驱动板接收数据；不要在正常速度闭环中同时调用两者，以免抢走反馈字节。

### 已知问题与验证边界

- `APP/line.c` 的 `Line_Read()` 使用 `uint32_t raw[4]` 保存 GPIO 原始掩码，避免 PA24～PA27 的高位被截断；后续修改须保留完整位掩码，或先比较是否为零再存布尔值。
- GPIO 读数位宽已修复，默认巡线系数为 `0.1`；当前使用最后可靠偏差保持、80/200 ms 间隙/寻线窗口和连续 2/3 次确认。这些源码状态及主机测试不代表实车巡线效果已验证。
- 编译可以验证构建兼容性，不能验证驱动板型号、真实接线、电机方向、传感器排列或参数效果。报告中明确区分源码检查、编译验证和硬件验证。

### 构建、HEX 交付与手动烧录

- 固件修改使用 CCS Project MCP 的 `buildProject` 构建 `empty_LP_MSPM0G3507_nortos_ticlang`，不得直接运行 make/gmake。构建前查询调试会话；若该工程正在调试，先按工具要求结束会话再构建。
- 确认编译和链接实际成功后再报告结果；出现 `taskId` 时通过 `waitForResult` 等待最终结果，不把工具受理任务当成构建通过。
- 用户使用 **UniFlash 手动烧录 HEX 文件**。默认工作流为：修改源码 → CCS 编译并生成 HEX → 报告构建结果与 HEX 路径 → 用户自行烧录。不要将 CCS 调试下载作为默认后续步骤。
- 主要交付物为工程内 `Debug/empty_LP_MSPM0G3507_nortos_ticlang.hex`，完整路径为 `E:/Victory/study/embedded/EDC25E/workspace/MSPM0_Projects/empty_LP_MSPM0G3507_nortos_ticlang/Debug/empty_LP_MSPM0G3507_nortos_ticlang.hex`；同名 `.out` 是链接及调试产物。报告时明确给出供 UniFlash 使用的 `.hex` 文件链接。
- 编译后确认 HEX 转换步骤成功、目标 `.hex` 存在且非空，结合构建日志和文件更新时间核对产物。增量构建无需重建时允许时间戳不变；不要通过修改时间戳冒充重新生成。编译、链接或 HEX 转换失败时，已有 HEX 可能是旧版本，不得作为本次成功产物交付。
- 不手工编辑生成产物，不无故移动或重命名 HEX，以便用户在 UniFlash 中使用固定路径；不删除或覆盖历史 ZIP。
- 默认完成编译、HEX 检查和结果报告即结束。除非用户明确要求，不自动启动调试、下载、运行、复位、连接串口或执行电机动作，也不自动启动或操作 UniFlash。不把编译成功表述为已烧录或已通过硬件验证。
- 纯文档修改检查内容准确性、引用路径和自动生成部分是否保留即可，无需重新编译。当前工作区根目录未初始化 Git，不假设 `git diff` / `git status` 可用，也不自行初始化仓库。
- 完成后简要说明改了什么、验证了什么、是否仍有与本次任务直接相关的限制；提供关键文件链接，避免粘贴整段构建日志。
