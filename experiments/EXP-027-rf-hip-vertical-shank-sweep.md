# EXP-027 CH0 正负端点后摆与小腿竖直保持

## Status

Testing：已完成纯 C 几何轨迹实现、host 数学验证、ESP32 构建和一次低幅实体尝试；该次
因 PCA9685 连续 NACK 被安全停止，轨迹未完成，不能视为实体验收通过。

## Objective

把已有的二维两连杆几何用于一个可实际运行的隔离命令：CH0 从显式给出的正端点向后
摆到负端点，同时让 CH1 持续补偿，使小腿相对地面保持向下竖直。该实验只接入隔离的
`servo-zero-test`，不接 BLE、FSM、正式执行器 adapter 或自动协调逻辑。

## Geometry

沿用 [`EXP-008`](EXP-008-two-link-foot-trajectory.md) 的坐标约定：`q_h` 是大腿相对
`+x` 的绝对角，`q_k` 是相对大腿角，小腿绝对角为 `q_h + q_k`。向下竖直要求：

```text
q_h + q_k = -π/2
q_k = -π/2 - q_h
```

因此大腿每转过一个角度，CH1 以相反的模型角度同步补偿。正运动学下足端为：

```text
x = 45 cos(q_h) + 70 cos(-π/2)
z = 45 sin(q_h) + 70 sin(-π/2)
```

轨迹端点和中间采样都重新计算 FK；`q_h + q_k` 偏离 `-π/2` 或脉宽超出软件范围时，
计划无效或动作停止。

## Command and parameters

```text
demo rf-hip-vertical POS_US NEG_US
stop
```

宿主机有界 runner（会先 `stop`，完成/异常后再次 `stop`）：

```text
python tools/servo_zero_rf_hip_vertical_trial.py 1600 1400 --port COM4
```

- `POS_US`、`NEG_US` 必须是操作者已经确认的 CH0 安全脉宽，均在 `500..2500 us`，且
  当前 RF 临时映射下 `POS_US > NEG_US`；命令不会把软件范围当作机械限位。
- 当前仅使用临时局部映射：CH0 `+14.6 us/°`、CH1 `-10.9 us/°`，模型基准为名义足端
  `(0,-100) mm` 对应的正膝支路和脉宽 `CH0/CH1=1494/1165 us`。这不是最终校准。
- 默认单向运动时间为 `4000 ms`，ESP32 每 `20 ms` 计算一次五次平滑步进；CH0/CH1
  通过一条连续的 PCA9685 auto-increment 帧同步提交。
- 启动前预载正端点及其 CH1 竖直补偿；由于 `/OE` 是全局使能，修复版把 CH2--CH15
  明确写成 `FULL_OFF`，再拉低 `/OE`，从寄存器层面只让 CH0/CH1 输出 PWM。非目标舵机
  会失去保持力矩，必须断开或由夹具支撑。完成负端点后自动拉高 `/OE`；中途 `stop`
  立即拉高 `/OE`，并恢复内存中的 CH0/CH1 起始目标。

## Acceptance criteria

1. host 数学测试在每个 `20 ms` 采样点证明 `q_h + q_k = -π/2`，CH0 从正端点单调
   下降到负端点，CH1 补偿目标在 `500..2500 us` 内。
2. 轨迹端点严格映射到传入的 CH0 脉宽，且不发生模型角度越界或脉宽越界。
3. 固件使用每次更新均包含 CH0/CH1 PWM 和 CH2--CH15 `FULL_OFF` 的单条原子帧；I²C
   失败会关闭输出。
4. `stop` 可在任一阶段中止，输出保持 `/OE HIGH`；实体动作前仍必须完成固定、限位、
   6 V 限流、人员净空和急停确认。

## Software evidence

- 新增 `servo_zero_leg_make_vertical_sweep_plan()` 和
  `servo_zero_leg_vertical_sweep_pose()`，将固定小腿绝对角与 FK 封装在纯 C 数学层。
- host 测试覆盖 `2000 -> 1000 us` 软件端点的映射、20 ms 单调性、竖直误差、CH1 补偿
  和最大脉宽跃度，以及零时长/相同端点拒绝。
- 严格 C host 命令通过：
  `cc -std=c11 -Wall -Wextra -Werror -Ifirmware-esp32/tools/servo-zero-test/main firmware-esp32/tools/servo-zero-test/main/servo_zero_math.c firmware-esp32/tools/servo-zero-test/main/servo_zero_leg_math.c firmware-esp32/tools/servo-zero-test/tests/test_servo_zero_math.c -lm -o /tmp/niulai-servo-zero-math-test && /tmp/niulai-servo-zero-math-test`。
- ESP-IDF v5.5.5 命令通过：
  `IDF_TOOLS_PATH=/tmp/niulai-idf-tools IDF_PYTHON_ENV_PATH=/tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env /tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env/bin/python /tmp/niulai-esp-idf-v5.5.5/tools/idf.py -B build-servo-zero build`；镜像大小 `0xd34b0`，应用分区余量 `17%`。
- `sh tools/run_offline_checks.sh` 通过，包含全套纯 C host、文档/实验/Decision 索引和
  `git diff --check`。

## Current hardware preflight

2026-09-13：在获准硬件上下文中，WSL `lsusb` 仍只显示 USB/IP 虚拟控制器，sysfs 和
`/dev` 没有 `ttyACM`/`ttyUSB`；宿主机 PowerShell 只读枚举到 `COM4`，宿主机 Python
`pyserial 3.5` 可用。通过 COM4 的有界 `chip_id` 握手确认 ESP32-S3 QFN56 rev v0.2，
随后按 `build-servo-zero/flasher_args.json` 使用三段镜像烧录，bootloader、app 和
partition table 均报告 `Hash of data verified`，并硬复位。启动日志再次确认隔离
`niulai_servo_zero_test`、PCA9685 `0x40 ACK`、50 Hz、`/OE HIGH`。

Operator 随后确认了实体动作安全条件，并用有界 runner 执行一次 `1600 -> 1400 us` 低幅
首轮。runner 收到启动回执，但约 `280 ms` 后 CH0/CH1 原子帧连续 NACK；固件自动拉高
`/OE` 并停止，轨迹未完成。串口统计为 `logical=3 attempts=8 success=2 nack=6
timeout=0 other=0 retries=5 recovered_after_retry=1 terminal_failures=1`；runner 随后发送
`stop` 并收到 `/OE HIGH`。原始记录见 [`EXP-027 trial log`](logs/EXP-027-2026-09-13-rf-hip-vertical-trial.txt)。

Operator 随后报告四条腿都在乱动；本 agent 立即停止所有后续测试，重新打开 COM4 等待启动
完成后发送 `stop`，收到 `UART stop -> /OE HIGH`。当前没有方向、垂直性、机械限位、电流、
压降、噪声或温升的有效实体验收证据；不得重复动作，下一步先隔离并排查 PCA9685/I2C/6 V
供电与其他通道状态。

## Diagnosis

- 已确认的软件事实：旧版 `vertical sweep pair frame` 只写 PCA9685 的 CH0/CH1 连续寄存器，且
  启动预载帧已经成功；因此 UART 参数解析和该帧的基本寄存器布局不是本次 NACK 的直接证据。
- 已确认的输出语义缺陷：PCA9685 `/OE`（GPIO1）是全局输出使能。命令将 `/OE` 从 HIGH 拉到
  LOW 时，会同时让已有 CH2--CH7 PWM 目标生效；“只发送 CH0/CH1 帧”并不等价于“只有
  CH0/CH1 会运动”。这可以解释 Operator 观察到的四条腿一起运动，也是本实验隔离设计的
  已知错误。
- NACK 的已确认时序是：启动扫描和预载成功，输出启用后动态 pair frame 出现
  `unexpected nack`；3 个逻辑事务共 8 次尝试、6 次 NACK，其中一次重试恢复、一次四次
  尝试后终止。基于既有 active-servo NACK 记录，供电压降、公共地/线束噪声或多舵机同时
  启动是当前主要假设，但本轮没有示波器/电流/电压证据，不能把它写成最终根因。
- 修复实现：新增 CH0/CH1-only 全帧生成器，每次写入 CH0/CH1 的 PWM，并把 CH2--CH15
  的 `OFF_H.FULL_OFF` 位置位；固件启动回执也必须包含 `CH0/CH1 PWM only`。这只解决
  “全局 `/OE` 使已有其它通道突然输出”的软件路径；非目标舵机失去保持力矩的机械风险
  仍由夹具/断开措施承担。

## Risk and rollback

这是扩大 CH0/CH1 联动运动范围的 `HIGH RISK` 隔离实验。`500..2500 us` 只是软件传输
范围，不是机械止挡；实体验证必须从已经量测的低幅安全端点开始。出现方向错误、碰限位、
异响、过流、掉压或发热时立即发送 `stop` 并切断 6 V。回滚方式是恢复此前的隔离固件镜像；
本命令不进入 BLE/FSM/正式执行器路径。

## Next step

修复版烧录后先确认启动日志中的 `CH0/CH1 PWM only, CH2-CH15 FULL_OFF`，再由 Operator
确认非目标舵机已断开或机械支撑，随后从 `1600 -> 1400 us` 做一次有界试验；任何非目标
动作、NACK、异常声响、掉压或发热都立即 `stop` 并切断 6 V。
