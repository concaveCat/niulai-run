# ESP32 + PCA9685 + 舵机：I²C/NACK 故障排查指南

Last reviewed: 2026-09-13

这是一份面向台架操作者的排查指南，整理自牛来快跑的 ESP32-S3、PCA9685
和 8 个舵机实验。它适用于“开发板、排母/载板、I²C 线和舵机驱动板组合后，
有时能动、有时 NACK 或完全无响应”的问题。

## 先看结论

遇到 NACK 时，先不要把它当成单纯的软件重试问题。应先区分三种状态：

| 日志现象 | 含义 | 首要检查 |
| --- | --- | --- |
| `ACK at 0x40` | 配置的 PCA9685 地址可见 | 才能进入动作测试 |
| `unexpected nack` | 一次事务没有收到从设备确认 | 供电、地、SDA/SCL 波形和瞬态 |
| `bus busy`、`probe timeout`、`transaction timeout` | 事务没有正常完成，可能有线被拉低或控制器卡住 | SCL/SDA 实际波形、接触和短路 |

本项目中最有区分度的事实是：

- ESP32 直接飞线到 PCA9685 时可以工作。
- ESP32 插入正确的排母路径时可以工作。
- 飞线接到错误的排母端口、SCL 接触不良、漏接 GND 或忘记给 PCA9685 供电时，
  启动扫描会出现无 ACK 和 timeout。
- 修正端口、共地并恢复 PCA9685 供电后，启动重新得到 `0x40 ACK`，随后 50 轮
  `pose pseudo 35 → stop` 全部完成：

  ```text
  logical=4833 attempts=4834 success=4833 nack=1 timeout=0 other=0
  retries=1 recovered_after_retry=1 terminal_failures=0
  ```

因此，“偶发 NACK”与“连接/接触/供电状态变化”可以同时存在；必须记录每次接线
变化，而不能只记录最后一次成功或失败。

## 1. 固定基线和安全边界

先把变量固定下来：

- ESP32-S3 隔离测试固件；本项目当前映射为 SDA=`GPIO11`、SCL=`GPIO12`、
  `/OE`=`GPIO1`。
- PCA9685 目标地址为 `0x40`，I²C 默认测试速率为 `400 kHz`。
- PCA9685 逻辑 VCC 使用约 3.3 V；舵机电源使用独立的约 6 V，并与 ESP32
  共地。不要把舵机大电流路径当作 I²C 地线的替代品。
- MPU6050/其他 I²C 从设备在定位 PCA9685 问题时先断开。
- 在发送动作前，确认机械固定、动作限位、6 V 电流/压降边界、人员净空和可立即
  断电；启动 ACK 探测本身保持 `/OE HIGH`，不会启动 PWM。

一次只改变一个因素。接线、供电、I²C 速率和动作幅度同时改变时，结果不能归因。

## 2. 第一步：做无动作启动 ACK 探测

用宿主机串口运行启动探测工具。USB 接在宿主机时，应在宿主机 PowerShell 中
运行 Python，而不是把 WSL 中不存在的 `/dev/ttyUSB*` 当成硬件未连接：

```bash
python tools/servo_zero_serial_probe.py \
  --port COM4 \
  --boot-wait 0.2 \
  --read-seconds 5
```

判定标准：

```text
I2C device ACK at 0x40 (configured PCA9685)
CH0-CH7 READY ...
PROBE_ACK=CONFIGURED_0x40
```

只有看到配置地址 `0x40` 的 ACK，才进入舵机动作测试。若日志是：

```text
I2C scan found no ACK
probe device timeout
I2C transaction timeout detected
initialization failed; /OE remains HIGH forever
```

就停止在启动诊断阶段，不要用动作测试“撞出”结果。

注意：PCA9685 板上的 LED 亮、VCC/SDA/SCL 静态测得 3.3 V，都不能代替启动
ACK。它们只能说明某些电源或上拉条件存在，不能证明动态 SDA/SCL 事务完整。

## 3. 第二步：区分连接错误和信号完整性

### 3.1 先确认实际 pin，而不是只看丝印位置

同时核对三层信息：

1. 固件配置的 GPIO 编号；
2. ESP32 排母的物理针脚和方向；
3. Gerber/PCB 网络最终落到的排母焊盘及 PCA9685 插座针脚。

GPIO11/12 与“排母第 11/12 个物理位置”不是自动等价的。排母从正面、背面
或上下翻转观察时，也很容易产生镜像误接。

### 3.2 断电测通断和短路

断电后逐项检查：

- ESP32 GPIO11 → PCA9685 SDA；
- ESP32 GPIO12 → PCA9685 SCL；
- ESP32 GND → PCA9685 GND；
- PCA9685 VCC → 3.3 V；
- SDA 对 GND、SCL 对 GND、SDA 对 SCL、SDA/SCL 对 3.3 V 不应有意外短路。

轻轻按压或晃动排母、飞线和焊点，重复测量。接触不良可能在静态万用表测试时
暂时导通，但在 I²C 事务期间表现为高电阻、间歇断路或边沿变形。

“蜂鸣器响了”只证明测试电流曾经通过，不证明：

- 导通电阻足够低；
- 连接在正确的网络上；
- 高低电平能在时序要求内传播；
- SDA 的从设备 ACK 能返回 ESP32。

### 3.3 使用旁路实验定位单根网络

保持 VCC、GND、/OE 和所有其他接线不变，分别做：

1. 只用临时线旁路 PCB 的 SDA；
2. 恢复 SDA，再只旁路 SCL；
3. SDA、SCL 都旁路。

每次都只做启动 ACK 探测。结果解释如下：

| 结果 | 说明 |
| --- | --- |
| 旁路 SDA 后恢复 | 原 SDA 网络、焊点、排母或回程可能有问题 |
| 旁路 SCL 后恢复 | 原 SCL 网络或排母接触重点可疑 |
| 两根都旁路才恢复 | PCB 两条网络、公共地或供电/上拉需要检查 |
| 两根都旁路仍失败 | 重点转向 GND、PCA VCC、地址、PCA9685 器件或供电状态 |

本项目曾遇到 SCL 在排母处接触不良；改善接触后仍失败的几次测试，随后发现
还存在接错端口或忘记供电的条件。每次实验必须同时记录接线和供电状态。

## 4. 第三步：区分 NACK 和 timeout

这两个现象不能混为一谈：

- NACK：主机完成了总线操作，但在应答位没有得到从设备确认。地址错误、SDA
  回程失败、器件未供电都可能造成它。
- timeout/bus busy：控制器等待某个总线状态或事务完成，可能是 SCL/SDA 被持续
  拉低、接触断续、波形严重失真或控制器状态受干扰。

因此，若日志同时大量出现 `probe timeout` 和 `bus busy`，优先检查物理层，
不要先增加重试次数。重试只能改变统计结果，不能修复被拉低的总线。

如果有示波器或逻辑分析仪，应直接在 PCA9685 端测量启动扫描：

- SCL 没有跳变：SCL 映射、接触、短路或主机配置异常；
- SCL 有跳变而 SDA 没有正常响应：SDA 回程/上拉/接触问题；
- SDA/SCL 都有完整跳变但 `0x40` 无 ACK：检查地址焊盘、VCC、GND 和器件；
- SDA 或 SCL 长时间为低：重点检查短路、从设备卡住和排母接触。

应在 PCA9685 引脚处观察，而不是只在 ESP32 端观察。两端静态都为 3.3 V，仍
不能证明事务中的上升沿和 ACK 位正确。

## 5. 第四步：检查供电和地

分别在 ESP32 端、排母两侧和 PCA9685 端测量：

- PCA9685 VCC-GND；
- ESP32 3V3-GND；
- 舵机 6 V-GND；
- 发生动作/启动瞬间的最低电压；
- 高电流舵机回路造成的公共地电位差。

静息电压正常不代表启动瞬间正常。曾经观察到舵机启动时总输入下降到约 2.4 V，
这类电源下陷足以导致控制器异常；更换电源后最低电压约 5.7 V，才恢复到可继续
测试的状态。

USB 供电、外部 3.3 V、载板 3.3 V 同时存在时，也要确认没有不期望的电源回灌。
做 A/B 时保持电源拓扑不变；不要把 USB 插拔、接线变更和 I²C 速率变更放在同一组
实验中。

## 6. 第五步：做速率 A/B，而不是猜测

在相同硬件、相同动作、相同供电下分别测试 100 kHz 和 400 kHz。至少记录：

- 轮数；
- `logical`；
- `attempts`；
- `success`；
- `nack`；
- `timeout`；
- `other`；
- `retries`；
- `recovered_after_retry`；
- `terminal_failures`。

本项目 50 轮结果为：

| 速率 | attempts | NACK | timeout | 最终失败 | NACK/attempts |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 100 kHz | 4859 | 14 | 0 | 0 | 0.288% |
| 400 kHz | 4862 | 15 | 0 | 0 | 0.309% |

两者几乎相同，因此这组条件下不能把 400 kHz 认定为主要故障变量。另一方面，
1 MHz 无舵机负载时无法完成稳定寄存器写入，说明更高速度确实暴露了物理层余量；
“100/400 kHz 相近”不等于“信号完整性永远没有问题”。

## 7. 第六步：做无 PWM 负载对照

要区分“舵机接着但不动”和“舵机动作瞬态”：

- A：Servo 6 V 开启，8 个舵机接着，`/OE HIGH`，不输出 PWM；
- B：相同硬件，`/OE LOW`，执行同一幅度动作。

两组都应统计 transaction 级数据，而不是只统计“多少轮出错”。若 A 为 0 NACK、
B 明显增加，才支持舵机启动/换向瞬态、电源下陷、公共地弹跳或线束耦合是主要
变量。若 A、B 接近，则回到 SDA/SCL/供电基础检查。

MPU6050 也应使用同样的单变量方法加入/移除。一次实验中断开 MPU6050 后 NACK
仍存在，说明 MPU6050 不是该故障的必要条件；这不能证明它在所有条件下都没有
影响，只能说明它不是唯一原因。

## 8. 通过后再做 50 轮动作测试

确认启动日志出现 `0x40 ACK` 后，且机械与电气安全条件满足，才运行有界 benchmark：

```bash
python tools/servo_zero_serial_benchmark.py \
  --port COM4 \
  --rounds 50 \
  --amplitude 35 \
  --timeout 8 \
  --output experiments/logs/EXP-021-direct-gpio-correct-port-50-rounds.log
```

该工具每轮执行 `pose pseudo 35 → 等待到位 → stop`，遇到完成等待超时会发送
`stop`，退出时也会尽力停止。最终必须报告完整统计，而不是只报告“50 轮中几轮
受影响”。建议用下面的模板：

```text
hardware:
power topology:
SDA/SCL/OE path:
I2C speed:
servo load:
MPU6050:
rounds:
logical:
attempts:
success:
nack:
timeout:
other:
retries:
recovered_after_retry:
terminal_failures:
raw log:
```

## 常见误判

- “PCA9685 LED 亮，所以 I²C 正常”：错误。LED 不能证明 ACK。
- “SDA/SCL 都是 3.3 V，所以线路正常”：错误。那只是空闲直流电平。
- “万用表测到导通，所以排母没问题”：错误。没有检查接触电阻、动态边沿和
  正确网络身份。
- “多重 retry 后成功，所以硬件没问题”：错误。retry 可能掩盖了边界故障；必须
  同时记录首次失败、重试次数和最终失败。
- “降低频率就一定解决”：不一定。它只能验证时序余量是否相关，不能修复错误
  pin、缺 GND、无供电或接触不良。
- “/OE 会影响 ACK”：通常不是。`/OE HIGH` 会禁止 PWM 输出，但 PCA9685 的
  I²C 地址应答仍应独立验证。

## 最短排查路径

```text
启动探测
  ├─ 0x40 ACK → 记录基线，再做负载/速率 A/B
  └─ 无 ACK
       ├─ timeout/bus busy → 查 SCL/SDA 波形、短路、接触、GND
       └─ 干净 NACK → 查地址、供电、SDA 回程、SCL 回程
                 ↓
       断电通断 + 旁路 SDA/SCL + PCA 端电压
                 ↓
       仍不确定 → 100/400 kHz A/B + 示波器/逻辑分析仪
                 ↓
       0x40 ACK 稳定后，才做 50 轮动作
```

相关原始实验：[`EXP-016`](../experiments/EXP-016-1mhz-i2c-boundary.md)、
[`EXP-017`](../experiments/EXP-017-20khz-shared-i2c-with-imu.md)、
[`EXP-018`](../experiments/EXP-018-100khz-vs-400khz-active-servo.md)、
[`EXP-019`](../experiments/EXP-019-wifi-400khz-50-rounds.md)、
[`EXP-020`](../experiments/EXP-020-usb-powered-400khz-50-rounds.md)、
[`EXP-021`](../experiments/EXP-021-direct-gpio-correct-port-50-rounds.md) 和
[`EXP-022`](../experiments/EXP-022-adjusted-wiring-ack-probe.md)。
