# 牛来快跑 PCB Controller 设计输入冻结 v0.4

状态：**Accepted Design Input / Operator 明确修订；尚未开始原理图或网表设计**

日期：2026-08-30

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md)

## 1. 权威性与合并规则

本文是 PCB Controller Rev A Circuit Design 当前唯一权威电气设计输入，并完整取代 v0.3。本文规范性纳入 v0.3、v0.2 和 v0.1 的全部未变条款；以下修订具有最高优先级，发生冲突时以本文为准。

本修订来自 Project maintainer 于 2026-08-30 的明确决定：以 `CH32V003F4P6 + 74HC244PW,118` 取代 v0.1 §4 接受的 `PCA9685PW,118`。旧冻结版本、BOM、Baseline、Decision、商城资料和聊天不得覆盖本文。

## 2. 八路舵机 PWM 架构修订

删除 `PCA9685PW,118` 及其 CH8–CH15 PWM 测试输出。八路舵机 PWM 的 Accepted architecture 改为：

```text
ESP32-S3 I2C
      |
      v
WCH CH32V003F4P6 -- 8 hardware-timed PWM --> Nexperia 74HC244PW,118 --> SERVO_PWM1..8
                                                    ^
                                                    |
                              TCA9539-controlled active-low OE
```

冻结要求如下：

- `CH32V003F4P6` 作为 I²C slave 接收 ESP32-S3 的舵机 PWM 命令；必须支持共享总线的 100/400 kHz 工作条件；exact 7-bit address、最小寄存器协议、PWM frame/range/resolution、atomic update 和 version/status register 由 Circuit Design 定义。
- 八路输出必须全部来自 CH32 的硬件 timer output-compare/PWM channels，不允许以普通 GPIO bit-banging 作为正常实现。
- `74HC244PW,118` 的两个 active-low output-enable 必须作为同一安全域控制，外部上拉使上电、reset、TCA9539 reset 或控制信号开路时默认 high/disabled；TCA9539 只允许在 safe-output initialization 和 servo-arm 条件满足后撤销禁止。
- CH32 reset、brownout、watchdog reset、未编程、固件异常或 PWM pins 为 high-impedance 时，74HC244 输入和舵机连接器侧必须由外部无源网络保持确定的 non-pulse/low 状态。
- 必须保留可访问的 CH32 `SWIO`、`NRST`、power 和 ground recovery/test points；不得依赖拆焊才能首次烧录或恢复。
- CH32 固件不属于 ESTOP 的安全执行元件。v0.3 §6 的硬件 ESTOP latch、`≤10 ms` servo-source shutdown 和 MASTER OFF→ON 恢复语义保持不变；任何 MCU 固件失效均不得阻止舵机电源被硬件切断。
- 74HC244 只缓冲 PWM logic，不承担舵机电源开关、反灌保护或故障能量切断。
- 原 v0.1 §5.2 “允许将 PCA9685 CH8–CH15 引作 PWM 测试焊盘”随 PCA9685 删除而取消，不要求以 CH32 额外 GPIO补回；8 路实际 PWM 仍须有可测量点。

## 3. 不变要求

除第 2 节明确修订的 PWM engine、相关测试输出和恢复接口外，v0.3 以及其规范性纳入的 v0.2/v0.1 条款全部保持不变，包括但不限于：

- ESP32-S3 主控及冻结 GPIO/扩展接口数量；
- 3S LiPo、电流/温度/压降和 3V3 包络；
- physical fuse、active protection、UBEC/USB source isolation；
- MASTER_ENABLE、外部常闭 ESTOP 及硬件锁存恢复；
- MCU reset、掉电或未武装时 PWM 与舵机电源默认关闭；
- Circuit Design Freeze 前必须清零 Design Input Blocker 和 Component Data Blocker；
- Circuit Design Freeze 首次状态只能是 Proposed / Review Candidate，AI 无权宣布 Accepted/Frozen/Ready for schematic。

## 4. 阶段与证据边界

本修订只改变权威电气设计输入，不授权 schematic、ERC、netlist、PCB layout、采购、下单或实体测试。CH32 的完整 pin assignment、74HC244 pin mapping、I²C protocol、boot/reset/watchdog/default-state analysis 和 firmware validation interface 均必须在 Circuit Design 中完成并进入 Proposed Circuit Design Freeze 候选。
