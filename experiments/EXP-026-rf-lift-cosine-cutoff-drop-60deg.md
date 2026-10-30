# EXP-026 右前腿 CH0 60° 余弦摆幅扩展

## Status

Testing：已完成代码扩展、host 预检、ESP-IDF 构建、COM4 烧录和一次成功的实体 60°
复测；机械端观察待 Operator 补录。仓库全量 offline gate 已通过；本次复核同时补齐
TCP 命令分帧 fake 与合并报文回归测试。

## Objective

在 EXP-025 的单腿时序不变的前提下，将 CH0 的名义余弦摆幅从 45° 提高到 60°，并在
中途截断，确认扩大幅度不会绕过 us 安全范围或破坏 `release → touchdown → stop` 路径。

## Safety decision

60° 是当前 motionleg 实验入口的最大名义摆幅，不是机械限位结论。固件在启用 `/OE`
之前检查抬升、完整摆动终点和下放候选点的局部 us 映射；任一点超出 `500..2500 us`
就拒绝启动。以此前串口读取的中立 CH0 `1746 us` 为基准，60° 末端会超出该范围，
所以实体测试必须先用已记录的四腿 support pose 建立 CH0 `1494 us` 等起始目标。

## Parameters

- CH1：`-491 us`，名义快速抬升 45°，约 `100 ms`；保持 `120 ms`；
- CH0：名义前摆 `60°`，`1800 ms` 余弦曲线，20 ms 更新；
- 截断：启动约 `1100 ms` 后发送 `release`，冻结当前 CH0；
- 下放：CH1 约 `120 ms` 受控回收；随后发送 `stop`，确认 `/OE HIGH`；
- CH2--CH7：保留 support pose 的目标，CH8--CH15：FULL_OFF。

## Command

```text
stop
pose pseudo 100
# 等待：rest pose reached; holding target; /OE LOW
stop
frame 1494,1165,1672,1748,1368,1279,1764,1934
demo rf-lift-cosine-drop 60
release
stop
```

对应 host helper：

```text
python tools/servo_zero_rf_lift_cosine_drop_trial.py --port COM4 --swing-degrees 60 --cutoff-after-start-ms 1100 --prepare-support
```

## Verification

Host 测试覆盖 5°..60° motionleg 计划、余弦四分之一点、释放锁存、120 ms 下放和 20 ms
脉宽步长；额外确认 60° 从 support frame 可映射，而从 `1746 us` 中立 CH0 基准会被
启动预检拒绝。实体结果、实际净空/方向/回地、限位、电流、噪声和温升待补录。

## Result

ESP-IDF v5.5.5 构建通过，隔离应用镜像为 `0xd3440`，bootloader、partition table 和
application 三段烧录均报告 `Hash of data verified`。第一次尝试使用 support pose 后直接
`stop`；固件按既有语义将软件目标恢复为中立值，60° 的启动预检因此拒绝，`/OE` 没有被
启用，I²C stats 为 `logical=0`。这证明了超范围保护生效。

修正 helper 为 `stop` 后显式写入 support frame
`1494,1165,1672,1748,1368,1279,1764,1934 us`，随后执行：

```text
python tools/servo_zero_rf_lift_cosine_drop_trial.py --port COM4 --swing-degrees 60 --cutoff-after-start-ms 1100 --prepare-support --drop-observe-seconds 0.8
```

成功运行的 UART 关键结果为：

```text
frame -> 1494/1165/1672/1748/1368/1279/1764/1934 us
HIGH RISK RF lift/cosine/drop started: CH1 fast lift=45.0 deg (-491 us) in 100 ms, hold=120 ms; CH0 cosine swing=60.0 deg over 1800 ms; send release to cut and drop knee
motionleg release at 1179 ms: hip frozen at swing=60.0 deg; accumulated knee recovery active
motionleg touchdown complete; hip held; /OE LOW; use stop
UART stop -> /OE HIGH
I2C stats: logical=68 attempts=68 success=68 nack=0 timeout=0 other=0 retries=0 recovered_after_retry=0 terminal_failures=0
```

这证明了 support frame 下 60° 路径通过 us 预检，余弦摆动可被中途截断，小腿完成回落，
总线无错误且安全停止。针对本次改动的纯 C math、脚本语法、`git diff --check`、
ESP-IDF 构建和全量 `sh tools/run_offline_checks.sh` 均通过；TCP 命令分帧回归同时覆盖
响应结束标记与合并报文。串口仍不能证明实际
净空、方向、触地点、限位余量、冲击、电流、噪声或温升；这些机械观察待 Operator 补录。

## Risk and rollback

这是扩大右前腿 CH0 运动范围的 `HIGH RISK` 实验。发现方向错误、拖地、碰限位、异响、
过流、掉压或发热时立即发送 `stop` 并切断 6 V。回滚是恢复上一版隔离镜像；命令不写 NVS。
