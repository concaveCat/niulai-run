# EXP-013 motionleg 按下—保持—释放单腿动作

## Status

Testing：隔离 servo-zero-test 实验路径，尚未接入 BLE/FSM。

## Behavior

`motionleg rf DEG` 将右前腿定义为 motionleg。当前试验参数为 CH1 以当前标定下的物理上抬方向约
`45°`（`-491 us`，按 `-10.9 us/°` 局部斜率取整）在 100 ms 内完成快速抬腿，等待 120 ms；随后 CH0 在 240 ms 内以余弦曲线前摆 45°，CH1 保持抬起角度。用户发送 `release`
后冻结 CH0；CH1 从抬腿位置按“抬腿角幅度 + 已冻结的大腿摆动角幅度”的累计角度回收，
在 120 ms 内受控释放到触地候选位置；`stop` 关闭输出。CH2--CH7 保持动作开始时脉宽，
CH8--CH15 才写 `FULL_OFF`。motionleg 更新只发送 CH0/CH1 连续原子帧。

## Verification

host 数学测试、ESP-IDF v5.5.5 构建和 `git diff --check` 通过。5°、10°、15°、20°
计划均可达；四档均完成串口 `motionleg rf DEG → release → stop` 闭环，日志确认未
release 前不自动回收，release 后触地完成并 `/OE HIGH` 收尾。实体抬高、速度、冲击、电流、
噪声和温升仍需 Operator 记录。

本轮将初始抬腿幅度固定为约 45°，并加入短等待，再让髋关节连续前摆；CH1 回收目标仍为
累计角度公式，motionleg 更新帧只覆盖连续的 CH0/CH1。修订后需重新进行四档实体回归。
此前四档均按 `stop → pose pseudo 100 → stop →
motionleg rf DEG → release → stop` 单独实体执行并正常收尾；串口结果证明链路和时序，
不替代 Operator 对实际高度、触地位置、卡顿、冲击、电流和温升的观察。

2026-09-13：按 `stop → pose pseudo 100 → stop → motionleg rf 45 → release → stop`
完成一次修订后实体回归。UART 确认从静息目标开始，CH1 抬腿 `45.0° / -491 us`，
CH0 前摆 `45.0°` 时 CH1 保持抬起角度，`release` 后按冻结髋角完成累计回收并以
`/OE HIGH` 收尾。I²C 统计为 `logical=210 attempts=212 nack=2 timeout=0 other=0`
、`retries=2 recovered_after_retry=2 terminal_failures=0`。实际抬高、方向、回地位置、
卡顿、冲击、电流、噪声和温升仍待 Operator 观察记录。

当前 motionleg 实验上限扩展为 60°；`step rf-ik` 的旧 20° 命令上限已移除，但模型可达性和脉宽检查仍保留。
