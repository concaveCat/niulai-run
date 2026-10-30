# EXP-035 舵机电源迁移后的无负载 50 轮 I²C 基线

## Objective

确认舵机电源线迁离控制板、无舵机负载条件下，PCA9685 400 kHz 总线的
NACK 和 timeout 基线。

## Hypothesis

移除舵机负载并调整舵机供电布线后，原先观察到的瞬态 NACK/timeout 会减少或消失。
本实验不能单独区分“无负载”和“供电线迁移”两个因素。

## Hardware version

- ESP32-S3 + PCA9685；启动扫描确认 PCA9685 `0x40 ACK`。
- Operator 报告舵机电源线已迁离 PCB；本轮按无舵机负载条件执行。
- MPU6500 `0x68` 仍被启动扫描发现，但不参与动作控制。

## Software commit

启动日志报告：`a2ce962-dirty`，编译时间 `2026-09-14 16:40:03`；本轮未烧录。

## Parameters

- I²C：`400000 Hz`；PCA9685 PWM：`50 Hz`。
- 每轮：`pose pseudo 35`，到达目标后 `stop`。
- 重复：50 轮；宿主机 Windows PowerShell / 原生 Python，通过 `COM4`。
- 固件 I²C 最大重试次数保持为 4；未修改频率、重试次数或固件。

## Test method

使用 `tools/servo_zero_serial_benchmark.py` 自动发送 `stop`、`stats reset`，
执行 50 次 `pose pseudo 35 → stop`，最后读取 `stats`。记录每轮完成标记、
I²C 错误、重试和最终失败。

## Video / logs

原始串口日志：[`EXP-035 raw log`](logs/EXP-035-2026-09-14-noload-50-rounds.log)。

## Result

事实结果：50/50 轮均收到 `rest pose reached` 并以 `UART stop -> /OE HIGH`
收尾。最终统计为：

```text
logical=4817 attempts=4817 success=4817 nack=0 timeout=0 other=0
retries=0 recovered_after_retry=0 terminal_failures=0
```

NACK 率为 `0/4817 = 0%`，timeout 率为 `0/4817 = 0%`，没有最终失败。
本轮证明当前无负载/新供电布线条件下总线基线稳定；不证明带舵机负载时的
供电压降、EMI、机械动作、摔倒或四足步态可靠性。

## Next step

在保持 I²C 频率、幅度和命令序列不变的前提下，恢复舵机电源并保持新布线，
重复同一 50 轮作为单变量负载对照；随后再决定是否继续四足步态测试。
