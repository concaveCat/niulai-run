# EXP-025 右前腿抬腿—髋余弦前摆—截断下放 demo

## Status

Testing：已完成 host 数学测试、代码实现、ESP-IDF 构建、COM4 烧录和一次实体 UART
演示；机械端观察待 Operator 补录。

## Objective

验证单腿动作按以下时序工作：小腿先快速抬起并保持净空；大腿随后以余弦曲线向前摆动；
Operator 在任意中途发送 `release` 截断轨迹；髋关节锁定截断位置，小腿立即开始回到触地
候选位置。

## Logic review

这个顺序适合单腿测试，因为髋摆动时小腿不会拖地，且释放时不需要等待髋摆动到预设终点。
“瞬间抬起”在物理舵机上不能实现为零时间脉宽跳变；本 demo 将它解释为约 `100 ms`
的快速有界抬升，以避免直接施加不可控的机械冲击。`release` 后没有额外等待，下一次
`20 ms` 控制 tick 即进入小腿下放；下放仍使用约 `120 ms` 的受控插值，以保持动作可观测
且可随时由 `stop` 中止。

## Hardware version

隔离 `servo-zero-test` 固件、ESP32-S3、PCA9685 `0x40`、50 Hz PWM、CH0/CH1 右前腿；
CH2--CH7 保持动作开始时的目标，CH8--CH15 为 FULL_OFF。实体演示前由 Operator 确认
机械固定、运动限位、6 V 限流、人员净空和急停。

## Parameters

- CH1 快速抬升：使用已有局部标尺 `-491 us`，名义约 45°，`100 ms`；
- 抬升后保持：`120 ms`；
- CH0 前摆：名义 45°，以余弦曲线运行 `1800 ms`，可由 `release` 在中途截断；
- 截断后：冻结当前 CH0，CH1 在 `120 ms` 内回收；回收目标按冻结髋角和已抬升角的累计
  脉宽/局部标尺计算；
- CH0/CH1 每 `20 ms` 使用一条连续的 9-byte PCA9685 auto-increment 帧更新。

## Command

```text
demo rf-lift-cosine-drop
release
stop
```

命令只允许在其他动作停止且输出关闭时启动；启动后只接受 `release`、`list`、`help` 和
`stop`。它是隔离 HIGH RISK 实验命令，不接入 BLE、FSM 或正式执行器路径。

## Verification

Host 测试检查了：抬腿阶段髋保持不变；余弦摆动四分之一点符合
`(1-cos(πt))/2`；释放时髋锁存当前采样；释放后小腿立即变化且在 `120 ms` 完成；
`1800 ms` 余弦摆动在 `20 ms` 采样下的局部脉宽变化不超过 `20 us`。ESP-IDF 构建、
COM4 三段镜像烧录均通过。本轮实体 UART 回报也完成了动作启动、截断、触地完成和
安全停止。

## Result

2026-09-13 实体 UART 演示使用原生 Windows `COM4`，启动前加载的目标为
`CH0=1746 us`、`CH1=1469 us`（其余通道保持各自当前目标）；启动日志确认 PCA9685
`0x40 ACK`、50 Hz、`/OE HIGH`。命令在启动后约 `700 ms` 的中段截断，固件记录
`motionleg release at 779 ms`，随后记录 `motionleg touchdown complete`，最后
`UART stop -> /OE HIGH`。I²C 统计为 `logical=47 attempts=47 success=47 nack=0
timeout=0 other=0 retries=0 recovered_after_retry=0 terminal_failures=0`。

本次证明了控制时序、截断锁存、下放完成和总线安全收尾；串口不能证明实际抬脚高度、
是否触地、方向、限位、冲击、电流、噪声或温升。另需注意：本次 demo 直接以启动时
加载的 CH0/CH1 目标为基准，没有在命令内部自动执行四腿 support pose，因此不能把本次
结果写成“已从标准静息位完成”的机械验收。完整原始日志见
[`EXP-025-2026-09-13-rf-lift-cosine-cutoff-drop.txt`](logs/EXP-025-2026-09-13-rf-lift-cosine-cutoff-drop.txt)。

## Risk and rollback

这是改变右前腿 CH1/CH0 运动范围和时序的 `HIGH RISK` 实验。任何方向错误、拖地、碰限位、
异响、过流、掉压或发热都应立即发送 `stop` 并切断 6 V。回滚方式是恢复当前已验证的
隔离固件镜像；本命令不改变 NVS。下一次实体复测前，应先用已验证的 support pose 建立
明确起始姿态，再重复同一截断点并补齐机械观察。
