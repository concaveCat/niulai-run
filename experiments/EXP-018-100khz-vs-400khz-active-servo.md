# EXP-018 100 kHz 与 400 kHz 带负载 I²C 对照

## Objective

在完全相同的 PCB、线束和舵机动作条件下比较 100 kHz 与 400 kHz I²C 的错误率，区分
I²C 时序/信号完整性余量与舵机动作瞬态造成的故障。

## Hypothesis

如果 400 kHz 的 NACK/timeout 显著多于 100 kHz，I²C 物理层余量是主要变量；如果两者接近，
则应优先排查舵机供电、公共地和动作瞬态。

## Hardware version

- ESP32-S3 rev 0.2，宿主机原生 PowerShell `COM4`，CH340 `1a86:55d3`。
- Operator 确认实际 Gerber 映射：SDA=`GPIO11`、SCL=`GPIO12`、OE=`GPIO1`；PCA9685=`0x40`。
- 8 个舵机全部接着，Servo 6 V 接通；MPU6050 断开；大电流供电线与杜邦线束保持并拢。
- 本轮未同步记录 Servo 6 V、3.3 V 或公共地波形/最低电压。

## Software commit

`8898101-dirty`，ESP-IDF v5.5.5。隔离 `servo-zero-test` 固件加入 `stats` 统计命令和
timeout 后 SDA/SCL 快照；两组均通过宿主机 COM4 三段镜像烧录，Hash 校验通过。

## Parameters

- PCA9685 PWM=`50 Hz`；`pose pseudo 35`；ESP32 内部动作节拍=`20 ms`；每轮约 `2520 ms`，
  轮末等待后发送 `stop`。
- I²C transaction 最多 `4` 次尝试，重试间隔 `1 ms`，单次驱动 timeout 参数 `100 ms`。
- 统计定义：`logical` 为一次 `i2c_master_transmit` 请求；`attempts` 为含重试的实际尝试数；
  `nack`、`timeout`、`other` 按每次失败尝试计数；`retries` 为实际发起的重试次数；
  `recovered_after_retry` 为至少一次失败后最终成功的逻辑 transaction 数；
  `terminal_failures` 为达到 4 次仍失败的逻辑 transaction 数。启动扫描不计入统计。

## Test method

1. 保持上述硬件状态，烧录目标 I²C 速率镜像，确认 PCA9685 `0x40 ACK`、`READY` 和 `/OE HIGH`。
2. 启动完成后发送 `stats reset`。
3. 分 5 批执行每批 10 轮 `pose pseudo 35 → stop`，不重置计数器；每批读取累计 `stats`。
4. 先完成 100 kHz 50 轮，再烧录 400 kHz 镜像，在相同条件下完成 400 kHz 50 轮。
5. 每次 `ESP_ERR_TIMEOUT` 由固件在返回后立即记录 SDA/SCL 输入电平；本轮没有 timeout，因此无电平快照。

## Video / logs

- 原始串口摘要与每批累计统计：[EXP-018-100khz-vs-400khz-active-servo.log](logs/EXP-018-100khz-vs-400khz-active-servo.log)。
- 无视频；实体动作是否平滑不由 UART 日志证明。

## Result

| I²C 速率 | 轮数 | logical | attempts | success | NACK | timeout | other | retries | recovered | terminal failure | NACK/attempts |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100 kHz | 50 | 4845 | 4859 | 4845 | 14 | 0 | 0 | 14 | 13 | 0 | 0.288% |
| 400 kHz | 50 | 4847 | 4862 | 4847 | 15 | 0 | 0 | 15 | 13 | 0 | 0.309% |

- 两组均 50/50 轮完成，均无最终失败；400 kHz 比 100 kHz 多 1 次 NACK，错误率差约
  `0.021` 个百分点，处于本次样本的随机波动量级。
- 两组均没有 software timeout，因此没有触发 SDA/SCL 快照；不能据此判断本次是否出现过
  SDA 或 SCL stuck low。
- 在本次并拢线束、舵机供电和 35% 动作条件下，100 kHz 与 400 kHz 的 transaction 级错误率
  基本相同。该结果不支持“降低到 100 kHz 就能显著消除当前 NACK”，也不支持把 400 kHz
  作为主要故障变量。

## Next step

优先在 PCA9685 端同时测量 SDA、SCL、3.3 V、Servo 6 V 和公共地，尤其捕获舵机启动/换向
瞬间的电压下陷与地弹；如需继续软件区分，再做 `/OE HIGH`、舵机 6 V 保持接通但不输出
PWM 的 dry-run 50 轮，与相同 transaction 负载的动作组比较。
