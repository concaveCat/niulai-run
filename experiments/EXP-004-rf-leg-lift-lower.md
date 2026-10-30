# EXP-004 右前腿平滑抬起/放下

## Objective

验证右前腿能够在不驱动其他七个通道的情况下，从已确认直立位平滑抬起、短暂停留、
缓慢放回，并在结束或故障时关闭 PCA9685 `/OE`。

## Hypothesis

CH1 膝关节先收、随后 CH0 髋和 CH1 膝使用五次 S 曲线同步运动，可以减少脚尖拖地与
启动/刹停冲击。该实验只验证单腿局部轨迹，不加入前后迈步、自动选腿或 gait planner。

## Hardware version

- ESP32-S3 N16R8；Carrier Gerber 映射 SDA=GPIO11、SCL=GPIO12、OE=GPIO1。
- PCA9685 地址 `0x40`、PWM 50 Hz；舵机电源为 Operator 已确认的独立 6 V，共地。
- 右前腿：CH0=`rf_hip`、CH1=`rf_knee`；CH2--CH15 在测试期间 `FULL_OFF`。
- 机械固定、限流、人员净空、可立即断开 6 V 的方式须在实体测试当天重新确认。

## Software commit

2026-09-12 基于分支 `fw/kw10-perfect-touchdown` 的未提交工作区实现；实体测试前记录
实际 commit 或明确标注 dirty 状态。

## Parameters

| Phase | Time | CH0 hip | CH1 knee |
| --- | ---: | ---: | ---: |
| upright settle | 250 ms | 1746 us | 1469 us |
| knee pre-lift | 100 ms | 1746 us | 1469 → 1300 us |
| lift | 500 ms | 1746 → 1494 us | 1300 → 1165 us |
| apex hold | 150 ms | 1494 us | 1165 us |
| lower | 700 ms | 1494 → 1746 us | 1165 → 1469 us |
| final settle | 250 ms | 1746 us | 1469 us |

- 更新周期：20 ms。
- 插值：`10t³ - 15t⁴ + 6t⁵`，阶段端点速度和加速度均为零。
- CH0/CH1 目标 `1494/1165 us` 来自本轮已确认的大幅度静态探针；它们是单点外推的实验
  脉宽，不冻结为角度、机械限位或最终 Servo 标定。

## Acceptance criteria

1. 启动保持 `/OE HIGH`；只有明确的 `step rf` 才开始一次动作。
2. 动作期间 CH2--CH15 保持 `FULL_OFF`，只写 CH0/CH1。
3. CH0/CH1 以 20 ms 周期按表中轨迹运动，结束回到 `1746/1469 us` 并自动 `/OE HIGH`。
4. UART `stop` 可在轨迹期间立即拉高 `/OE`；任一 I2C 错误同样停止输出。
5. 实体观察无撞限位、明显抖动、异常噪声、过流或快速温升。

## Test method

1. 断开 6 V 完成构建、烧录和启动日志检查，确认 `0x40` ACK、50 Hz、`/OE HIGH`。
2. 固定右前腿，确认保险/限流、人员净空和可立即断电；其他腿不应获得 PWM。
3. 标准前置流程为 UART `stop`，再发送一次 `pose pseudo 100`，让四条腿按两组对角腿错开约 120 ms、20 ms 原子整帧缓慢进入静息位。
4. 静息位到达后发送 `stop` 回到中立位，再仅发送一次 `step rf [PCT]`，同步观察动作、供电电流和 UART。
5. 出现干涉、顶死、抖动、异常噪声或电流时立即发送 `stop` 并切断 6 V；结束后再次发送 `stop`。
6. 保存 UART 原始日志，记录是否抬脚、最高点姿态、落地冲击、电流峰值和温升。

单腿动作的通道约束：CH0/CH1 为右前腿动作通道；CH2--CH7 必须在每个动作帧保持
动作开始前的姿态脉宽以承重；只有 CH8--CH15 写入 `FULL_OFF`。不得为了“单腿”而关闭
其他活动腿。

## Result

2026-09-12：纯 C 数学测试以 `cc -std=c11 -Wall -Wextra -Werror` 编译并通过；电脑端
5 项 Python 测试和 `git diff --check` 通过。使用 ESP-IDF v5.5.5 执行
`idf.py -B build-servo-zero build` 成功，生成 `niulai_servo_zero_test.bin`，大小
`0x408e0` 字节，app 分区余量 75%。随后烧录成功，启动日志确认 `0x40` ACK、50 Hz 和
`/OE HIGH`。Operator 将垂直站立定义为八路中立位；固件以较慢节奏重发并保存
`CH0..CH7 = 1746,1469,1427,1415,1619,1582,1449,1600 us`，`run` 日志确认
`/OE LOW` 约 2.5 s 后由 `stop` 恢复 `/OE HIGH`。同次 `step rf` 日志确认
`CH0 1746->1680 us`、`CH1 1469->1360 us`，总时长 1950 ms，完成后恢复直立值并拉高
`/OE`。UART 证据完整，但尚未记录 Operator 对实际抬脚、轨迹平滑度、限位、电流或温升的
观察；实体验收仍未完成。

2026-09-12：在 Operator 逐项确认机械固定、运动限位、6 V 供电/电流边界、人员净空和
急停方式后，先发送 `list` 确认 UART 响应，再仅发送一次 `step rf 25`。原始回显为
`RF step started: amplitude=25%, CH0 1746->1729 us, CH1 1469->1442 us, 1950 ms`，
随后为 `RF step complete; upright restored; /OE HIGH`；动作窗口结束后再次发送 `stop`，
回显 `UART stop -> /OE HIGH`。本次未报告 I²C NACK，但现场是否抬脚、拖地、异响、撞限位、
电流/压降和温升仍待 Operator 观察；该次测试使用现有脉宽候选轨迹，不是 EXP-008 的
45/70 mm IK 轨迹。

2026-09-12：随后在相同安全条件下仅发送一次 `step rf 100`；UART 回显
`RF step started: amplitude=100%, CH0 1746->1680 us, CH1 1469->1360 us, 1950 ms`、
`RF step complete; upright restored; /OE HIGH`，再发送 `stop` 收尾。Operator 观察到实体
基本没有动作。该失败结果说明当前候选脉宽位移不足或方向/机构映射仍不匹配；不能从 UART
完成日志推断腿已抬起，也不能继续扩大到正式控制链路。

2026-09-12：在 Operator 明确授权直接外推大幅度后，执行单腿 100% 静息目标探针
`pose rest 100 0,1`。固件先保持直立约 400 ms，再以 20 ms 轨迹将 RF 髋/膝送至
CH0 `1494 us`、CH1 `1165 us`，并保持目标；CH2--CH7 保持直立值
`1427,1415,1619,1582,1449,1600 us`。UART 回显 `rest pose reached; holding target;
/OE LOW`，未出现 I²C/NACK；随后发送 `stop`、`load`，回显 `/OE HIGH`，`list` 确认
CH0--CH7 恢复为 `1746,1469,1427,1415,1619,1582,1449,1600 us`。Operator 已确认本次
实体动作完成；具体弯曲角度、抬脚高度、硬限位、电流、噪声和温升未量化记录。

2026-09-12：将 `step rf` 的实验目标更新为 CH0 `1494 us`、CH1 预弯 `1300 us`、
CH1 终点 `1165 us` 后，重新通过纯 C host 测试和 ESP-IDF v5.5.5 构建；镜像大小为
`0x44a90`，app 分区余量约 73%。按烧录元数据写入 bootloader、app 和 partition table，
三段均报告 `Hash of data verified` 并硬复位。随后执行一次 `step rf 100`；UART 回显
`RF step started: amplitude=100%, CH0 1746->1494 us, CH1 1469->1165 us, 1950 ms`，
并回显 `RF step complete; upright restored; /OE HIGH`，全程无 I²C/NACK。最后发送
`stop`、`load`，`list` 确认 CH0--CH7 恢复直立值；实体抬脚高度、前摆幅度、是否撞限位、
电流、噪声和温升仍待 Operator 观察记录。

## Next step

由 Operator 补充本次大幅度 `step rf 100` 的实际抬脚高度、前摆幅度、是否平滑、是否撞
限位，以及电流、噪声和温升；未满足验收标准前不接入正式控制链路。

2026-09-12：Operator 确认后续直接使用伪同步四腿静息位且当前不预计发生硬件限位问题。按标准流程执行
`stop → pose pseudo 100 → stop → step rf 25 → stop`；UART 确认伪同步 V2 使用
`0xc3/0x3c` 对角腿掩码、120 ms 错相、2000 ms minimum-jerk、20 ms 原子整帧，随后
`step rf 25` 以 CH0 `1746→1683 us`、CH1 `1469→1393 us` 完成并恢复 `/OE HIGH`。
本次固件链路无 I²C 错误；实体动作质量、电流、噪声和温升仍需 Operator 记录。

2026-09-12：Operator 指出单腿动作期间 CH2/3、CH4/5 不应发生非预期动作。修正版取消
RF step 对 CH2--CH7 的 FULL_OFF 写入，改为每个 20 ms 动作周期发送完整原子帧：CH0/1
更新轨迹，CH2--CH7 保持动作开始前脉宽，CH8--CH15 才写入 FULL_OFF。host 数学测试、
ESP-IDF 重建和 `git diff --check` 通过；修正版烧录三段均 Hash 校验通过，并完成一次
`list → step rf 25 → stop` 回归，无 I²C 错误。

2026-09-12：在 Operator 明确确认机械固定、限位、6 V 供电/电流边界、人员净空和急停条件后，
通过宿主机 PowerShell 原生 `COM4` 执行 `stop → step rf 25 → stop`。UART 回显动作启动目标
`CH0 1746->1683 us`、`CH1 1469->1393 us`、时长 `1950 ms`；约 10 ms 后在 active hold
atomic frame 出现 `I2C hardware NACK`、`ESP_ERR_INVALID_STATE`，固件自动 `/OE HIGH` 并停止，
随后 `stop` 收尾成功。未收到 `RF step complete`，未重试或扩大动作；现场是否发生短暂姿态变化、
电流/压降、噪声和温升仍需 Operator 记录。该失败结果要求先排查 PCA9685/共享 I²C 供电、公共地、
连接和负载瞬态，再进行任何实体动作。

最新下一步：保留故障日志，完成供电/公共地/连接与 I²C 瞬态排查；在原因未定位前不重复实体 `step rf`。
