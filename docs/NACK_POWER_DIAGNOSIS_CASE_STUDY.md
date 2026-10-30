# 从 NACK 到供电：一次 I²C 舵机故障的实验复盘

Last reviewed: 2026-09-13

这不是一份抽象的 I²C 教程，而是牛来快跑一次真实排障过程的记录。问题表现为：
开发板直接连接 PCA9685 时能够工作，接入载板/排母和 8 个舵机后，动作期间出现
NACK，偶尔还出现 software timeout。目标不是凭现象猜根因，而是通过重复、对照和
完整 transaction 统计，逐步把嫌疑从“软件/速率/IMU/串扰”收敛到“供电、公共地和
互连的物理条件”。

## 一开始我们知道什么

当时的硬件事实是：

- ESP32-S3 使用 PCA9685 的 `SDA=GPIO11`、`SCL=GPIO12`、`/OE=GPIO1`；PCA9685
  目标地址为 `0x40`。
- PCA9685 逻辑侧约 3.3 V，舵机侧独立约 6 V，二者需要共地。
- 单通道和较少通道动作可以工作；8 个舵机同时动作时更容易出现 I²C NACK。
- 静态测量并不异常：某次无舵机负载测得 `+6V-GND≈25 kΩ`，
  `EXT_6V_IN=SERVO_6V≈5.99 V`，ESP32 输出约 `3.29 V`。
- 但舵机启动瞬间曾观察到总输入电压掉到约 `2.4 V`。更换电源后，动作期间
  观察到的最低电压约 `5.7 V`。

这已经提示一个关键区别：万用表测到的静态电压正常，不代表舵机启动、换向和多路
同时运动时的瞬态供电正常。

## 实验一：先拉开大电流线，再尝试 20 kHz

最初的想法有两个：

1. 把舵机大电流供电线与 I²C 杜邦线束空间分开，降低可能的电磁耦合；
2. 把 I²C 降到 20 kHz，给较差的信号完整性更多余量。

测试仍使用 PCA9685 50 Hz PWM 和 8 路整帧更新。每个动作周期约 20 ms，失败允许
最多 4 次尝试，每次重试间隔约 1 ms。

### 20 kHz、MPU6050 接入

先在舵机 6 V 已接通、MPU6050 仍接入时测试：

- 35% 幅度动作 4 轮：其中 3 轮出现至少一次 active-frame 首次尝试 NACK；
- 100% 幅度动作 1 轮：出现至少 8 次首次尝试 NACK；
- 这些失败都在有界重试后继续完成，没有 4 次连续失败导致的最终中止。

这个结果说明 20 kHz 并没有自动消除问题。它同时留下了一个合理但未验证的嫌疑：
MPU6050 可能增加了总线电容、额外上拉或线束耦合。

### 20 kHz、断开 MPU6050

保持舵机供电和其余条件不变，只断开 MPU6050：

- 启动扫描只剩 PCA9685 `0x40`；
- 35% 动作 4 轮中，前 3 轮无 NACK，第 4 轮仍出现 3 次首次尝试 NACK；
- 所有轮次仍然完成并安全 `stop`。

这是一个非常重要的排除：MPU6050 不是 NACK 的必要条件。它可能让余量更差，但不能
解释没有它时仍然存在的故障。

### 为什么 20 kHz 也不是当前玩法的可靠答案

一帧 PCA9685 八通道写入约为：

```text
(地址字节 + 33 个数据字节) × 9 个 SCL 周期 ≈ 306 个 SCL 周期
```

因此理想占线时间约为：

| I²C 速率 | 一帧理想占线时间 | 对 20 ms 刷新周期的余量 |
| ---: | ---: | ---: |
| 100 kHz | 3.06 ms | 16.94 ms |
| 20 kHz | 15.30 ms | 4.70 ms |
| 10 kHz | 30.60 ms | 不足以每 20 ms 完成一帧 |

在 20 kHz 下，一次重试还会再占用约 15.3 ms，并增加调度开销，几乎立即破坏
20 ms 原子刷新周期。因此低速可以作为诊断变量，却不能在当前整帧玩法中掩盖物理
问题。

证据记录：[`EXP-017`](../experiments/EXP-017-20khz-shared-i2c-with-imu.md)。

## 实验二：大电流线分开与并拢，各做 20 轮

为了测试“串扰”而不是只凭感觉观察线束，我们固定以下条件：

- MPU6050 断开；
- 舵机 6 V 接通，8 个舵机连接；
- `pose pseudo 35`；
- I²C 400 kHz；
- 每轮到达 `rest pose reached` 后发送 `stop`；
- 分开和并拢各 20 轮。

### 线束空间分开

结果：18/20 轮没有观察到 I²C 错误，2/20 轮各出现一次首次尝试 NACK；重试均恢复。

### 线束并拢

结果：18/20 轮没有观察到 I²C 错误，2/20 轮受影响，但故障更重：

- 一轮出现 software timeout；
- 另一轮出现两次连续硬件 NACK，随后重试恢复。

这组结果很有价值，但不能夸大：受影响轮次都是 2/20，样本不能证明“并拢一定
增加发生率”。它支持的是更谨慎的判断：并拢可能增加故障严重度，但串扰尚未被
单独确认。下一步仍应同时观察 SDA、SCL、3.3 V、6 V 和公共地波形。

证据记录：[`EXP-017`](../experiments/EXP-017-20khz-shared-i2c-with-imu.md)。

## 实验三：100 kHz 对 400 kHz，各做 50 轮

这是整个过程的关键实验，因为它把“偶尔有影响”转换成了 transaction 级统计。
我们没有只记录“50 轮里几轮异常”，而是记录每一次逻辑事务和包含重试的物理尝试：

- `logical`：逻辑 I²C transaction 数；
- `attempts`：包括重试在内的实际尝试数；
- `nack`：NACK 失败尝试数；
- `timeout`：software timeout 尝试数；
- `retries`：实际发起的重试数；
- `recovered_after_retry`：失败后最终成功的逻辑事务数；
- `terminal_failures`：四次尝试后仍失败的逻辑事务数。

条件完全相同：MPU6050 断开、8 个舵机供电并连接、线束并拢、35% 幅度动作，各 50 轮。

| 速率 | 轮数 | logical | attempts | success | NACK | timeout | retries | 重试后恢复 | 最终失败 | NACK/attempts |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100 kHz | 50 | 4845 | 4859 | 4845 | 14 | 0 | 14 | 13 | 0 | 0.288% |
| 400 kHz | 50 | 4847 | 4862 | 4847 | 15 | 0 | 15 | 13 | 0 | 0.309% |

两组都 50/50 完成，错误率只相差 0.021 个百分点，而且都没有 timeout 或最终失败。
所以在这组带负载动作条件下，降低到 100 kHz 没有显著降低 NACK；I²C 速率不是当前
故障的主要区分变量。

这个结论并不表示信号完整性无关：此前 1 MHz 在无舵机负载时就无法稳定完成寄存器
写入，而 400 kHz 可以。它表示的是：当前常用的 100/400 kHz 区间内，带舵机动作的
NACK 数量没有随速率明显变化；1 MHz 暴露的是另一条更高速的物理层边界。

证据记录：[`EXP-016`](../experiments/EXP-016-1mhz-i2c-boundary.md)、
[`EXP-018`](../experiments/EXP-018-100khz-vs-400khz-active-servo.md)。

## 实验四：USB 拔除、Wi‑Fi 远程诊断的 50 轮

为了排查“USB 电源或 USB 路径是否参与故障”，我们曾做过一组 USB 拔除条件：

- ESP32 从外部 3.3 V 供电；
- USB 拔除；
- 舵机 6 V 接通；
- 8 个舵机连接；
- MPU6050 断开；
- 400 kHz；
- 通过 ESP32 SoftAP/TCP 获取日志；
- 50 轮动作。

Operator 回报：

```text
logical=4796 attempts=4796 success=4796 nack=0 timeout=0 other=0
retries=0 recovered_after_retry=0 terminal_failures=0
```

这个结果说明在那组供电和运行条件下没有复现 NACK，但它不能单独证明“双 3.3 V
电源”就是根因，因为 Wi‑Fi 改变了射频活动和功耗，且完整逐轮原始日志没有归档。
它是一个很好的对照，不是最终定论。

证据记录：[`EXP-019`](../experiments/EXP-019-wifi-400khz-50-rounds.md)。

## 实验五：USB 供电对照暴露了“物理前提”的重要性

随后改成 ESP32 由电脑 USB 供电，其他部分按预期接线。启动扫描却失败：

```text
I2C scan found no ACK
I2C bus is still busy but software timeout detected
MODE1 ... addr=0x40
initialization failed; /OE remains HIGH forever
```

当时如果直接把它解释成“USB 供电导致 NACK”，会过早下结论。之后逐项复查发现，
测试过程中还混入了多个物理变量：

- 共地曾经接错或漏接；
- SDA/SCL 曾接到错误端口；
- PCA9685 逻辑供电曾经忘记接通；
- SCL 在排母处存在接触不良；
- SDA/SCL 即使万用表测得导通，也不代表动态事务可靠。

修正其中的共地、端口和供电条件后，启动日志恢复：

```text
I2C device ACK at 0x40 (configured PCA9685)
CH0-CH7 READY at 50 Hz PWM / 400000 Hz I2C
```

这段过程的意义不是“USB 没问题”或“USB 有问题”，而是证明：如果启动阶段都
没有稳定的 `0x40 ACK`，后面的动作 NACK 统计没有意义；很多看似随机的 I²C 故障，
首先可能只是测试前提没有真正满足。

证据记录：[`EXP-020`](../experiments/EXP-020-usb-powered-400khz-50-rounds.md)、
[`EXP-022`](../experiments/EXP-022-adjusted-wiring-ack-probe.md)。

## 最终验证：修正供电和接线后重新做 50 轮

在以下条件成立后，才再次进行动作测试：

- PCA9685 已供电；
- ESP32 与 PCA9685 共地；
- SDA/SCL/OE 端口正确；
- 排母接触和端口问题已修正；
- 启动扫描先确认 `0x40 ACK`；
- 机械限位、固定、电流边界、净空和急停已确认。

执行 400 kHz、35% 幅度、8 个舵机、50 轮 `pose pseudo 35 → stop`：

```text
logical=4833 attempts=4834 success=4833 nack=1 timeout=0 other=0
retries=1 recovered_after_retry=1 terminal_failures=0
```

50/50 轮均到达目标并安全 stop；唯一一次 NACK 在一次重试后恢复。
NACK 率为 `1/4834≈0.021%`，没有 timeout 和最终失败。

证据记录：[`EXP-021`](../experiments/EXP-021-direct-gpio-correct-port-50-rounds.md)。

## 最终能下什么结论

### 已被证据支持的结论

1. 软件命令路径、烧录偏移和 PCA9685 固定地址不是主要故障。正确接线和供电下，
   启动稳定看到 `0x40 ACK`，50 轮也能完成。
2. MPU6050 不是 NACK 的必要条件。断开它以后故障仍可出现。
3. 100 kHz 相比 400 kHz 没有在当前带负载条件下显著降低 NACK；不能把“降速”当作
   主要修复方案。
4. 线束并拢可能加重故障，但 20/20 对照的发生率相同，串扰没有被单独证明。
5. 供电、公共地和连接质量是最主要的剩余故障范围。尤其是实测到的舵机启动瞬间
   约 2.4 V 下陷，是比静态电压更有力的物理证据。

### 仍然不能声称的结论

- 不能说所有 NACK 都由舵机电源造成；SCL 排母接触、错误端口、漏接 GND 和忘记
  PCA9685 供电也确实能造成启动无 ACK/timeout。
- 不能说串扰已经被排除；只是在当前 20 轮样本中没有得到发生率差异。
- 不能说重试解决了根因。重试只让 `recovered_after_retry` 增加，必须保留首次
  NACK 和 timeout 的统计。
- 不能用“PCA9685 LED 亮”或“空闲 SDA/SCL 是 3.3 V”代替动态波形和 ACK 验证。

最准确的工程表述是：

> 当前 NACK 问题的主嫌疑已经从软件和 MPU6050 收敛到舵机供电瞬态、公共地/地弹、
> 排母及信号连接质量；其中供电下陷已有直接电压观测，串扰和具体供电路径贡献仍需
> 在 PCA9685 端用示波器同步捕获后再定量区分。

## 这次排查最值得复用的方法

### 先设“启动 ACK 闸门”

没有 `0x40 ACK` 就不进入动作测试。这样可以把“器件根本没接通”和“动作期间
瞬态出错”分开，避免用无效动作日志解释硬件连接问题。

### 统计 transaction，不统计“感觉有几轮异常”

一轮动作包含很多 I²C transaction。`1/50 轮受影响`可能对应 1 次 NACK，也可能
对应几十次 NACK 和 timeout。只有同时记录 `logical/attempts/nack/timeout/retries`
才能比较不同条件。

### 每次只变一个变量，但先记录所有前提

“拔 USB”“改 3.3 V”“断 MPU”“拉开线束”“换频率”如果一起发生，结果只能说明
整组条件变了，不能说明哪个变量起作用。尤其要在实验记录中写清楚 GND、PCA VCC、
舵机 6 V 和排母/飞线状态。

### 区分静态测量和瞬态测量

`5.99 V` 的静态读数不能否定启动时的 `2.4 V` 下陷；`3.3 V` 的空闲 SDA/SCL
不能证明 ACK 位、上升沿和 SCL 释放正常。最终仍应在 PCA9685 端同步观察：

```text
SDA、SCL、PCA VCC、舵机 6 V、ESP32/PCA 公共地
```

### 先修复物理前提，再优化软件

有界重试和自动 `/OE HIGH` 是必要的安全保护，但不能替代供电、接地、排母接触和
信号波形修复。只有物理前提稳定后，50 轮统计才具有诊断价值。

## 原始记录入口

- [`EXP-017`](../experiments/EXP-017-20khz-shared-i2c-with-imu.md)：20 kHz、MPU6050、线束分开/并拢。
- [`EXP-018`](../experiments/EXP-018-100khz-vs-400khz-active-servo.md)：100/400 kHz 各 50 轮。
- [`EXP-019`](../experiments/EXP-019-wifi-400khz-50-rounds.md)：USB 拔除、Wi‑Fi 远程诊断。
- [`EXP-020`](../experiments/EXP-020-usb-powered-400khz-50-rounds.md)：USB 供电和启动 ACK 对照。
- [`EXP-021`](../experiments/EXP-021-direct-gpio-correct-port-50-rounds.md)：修正条件后的 50 轮验证。
- [`EXP-022`](../experiments/EXP-022-adjusted-wiring-ack-probe.md)：错误接线和物理前提暴露。
- 通用操作版：[I²C/舵机排查指南](I2C_SERVO_TROUBLESHOOTING_GUIDE.md)。
