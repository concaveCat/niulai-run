# 牛来快跑 PCB Controller 设计输入冻结 v0.5

状态：**Accepted Design Input / Operator 明确修订；尚未开始原理图或网表设计**

日期：2026-08-30

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md)

## 1. 权威性与合并规则

本文是 PCB Controller Rev A Circuit Design 当前唯一权威电气设计输入，并完整取代 v0.4。本文规范性纳入 v0.4、v0.3、v0.2 和 v0.1 的全部未变条款；以下修订具有最高优先级，发生冲突时以本文为准。

本修订来自 Project maintainer 于 2026-08-30 的明确决定：采用 `Silergy SY8388ARHC / JLC C5110279` 板载同步降压作为默认 6.0 V 舵机电源，并保留默认 DNP 的外置 UBEC fallback。旧冻结版本、BOM、Baseline、Decision、商城资料和聊天不得覆盖本文。

## 2. 6 V 舵机电源架构修订

v0.2 接受的 YSIDO 外置 UBEC 不再是默认舵机电源。默认 assembled architecture 改为：

```text
3S LiPo
  → physical fuse
  → active battery protection
  → SERVO_BUCK_IN
  → Silergy SY8388ARHC synchronous buck
  → BUCK_6V
  → servo-source isolation / protection / current sense
  → SERVO_V+
```

冻结要求如下：

- exact converter MPN 固定为 `SY8388ARHC`，ordering item=`C5110279`，package=`QFN2.5×2.5-16`；不得以 `SY83088ARHC`、`SY8388RHC` 或其他相近后缀静默替代。
- 板载 buck 输出 nominal 固定为 `6.0 V`，用于正常 assembled configuration 的舵机电源。
- 板载 buck 及其输入/输出导电路径必须满足 `6.0 A continuous` 和 `8.0 A for up to 1.0 s`，相邻 8 A 事件起点至少间隔 `10 s`；v0.3 §2 的环境温度、温升、结温和板内压降边界保持不变。
- v0.3 §3.2 原 `VBAT_SWITCHED→UBEC` 输入支路包络改为 `active battery protection→SERVO_BUCK_IN` 支路包络，仍为 6.0 A continuous、7.5 A/1 s/≥10 s 以及 startup/inrush ≤10 A for 10 ms。超过该输入包络时，active protection 允许限流或确定性关断。
- Circuit Design 必须完成反馈精度、电感/ripple/饱和电流、输入/输出有效电容及 bias derating、输入 RMS ripple、MOSFET/IC/inductor/capacitor/PCB loss、热、启动、load transient、current-limit/UVP/OVP/OTP、反向电流、舵机 regeneration、EMI 和 fault recovery 分析。
- `EN` 必须纳入硬件 servo-source enable/ESTOP chain。外部常闭 ESTOP 触发后，v0.3 §6 的 `≤10 ms` servo-source shutdown、hardware latch 和 MASTER OFF→ON 恢复语义保持不变；不得依赖 ESP32、CH32 或其他 firmware 关闭 buck。
- MCU reset、掉电、未武装、ESTOP fault 或 enable-control open circuit 时，舵机电源必须保持确定的 off/default-safe 状态。
- `PG`、`MODE`、`ILMT`、`BYP`、feedback、输入/输出滤波和 exact branch-protection implementation 属 Circuit Design 正常工程决策，不由本 Design Input 预先选值。
- 板载 buck 把高电流开关电源的热、环路和 EMI 风险纳入 PCB；manufacturer typical application 不能代替本项目 worst-case calculation 和 prototype validation。

## 3. 外置 UBEC DNP fallback

保留 v0.2 定义的两套带卡扣 2P 外置 UBEC input/output connector footprint，作为 Rev A fallback，但默认 assembly 状态均为 DNP：

- fallback input：`VBAT_SWITCHED`/受保护电池电源与 GND；
- fallback output：外置 UBEC 6 V 与 GND；
- exact connector family、pin polarity和现有防误插要求保持 v0.2/Circuit Design 已记录约定；
- 默认板载 buck configuration 与外置 UBEC fallback 不得直接并联供电；Circuit Design 必须用明确的 DNP/0 Ω/selector/isolator assembly option 形成 mutually exclusive source configuration；
- assembly table、silkscreen 和 bring-up instructions 必须能区分 `ONBOARD_BUCK` 与 `EXTERNAL_UBEC_FALLBACK`，禁止同时装配会形成 source fighting、charging path 或绕过 ESTOP/protection 的组合；
- fallback 只有在相应 assembly option 被明确选择并按原 PVR-014 验证后才能使用；保留 footprint 不表示 YSIDO 能力已被纸面证明。

若未来决定删除 fallback footprint、允许并联运行或改变外部 connector/interface，必须再次修订权威 Design Input Freeze。

## 4. 不变要求

除第 2、3 节明确修订的 6 V source、外置 UBEC角色和相关接口装配状态外，v0.4 以及其规范性纳入的 v0.3/v0.2/v0.1 条款全部保持不变，包括但不限于：

- `CH32V003F4P6 + 74HC244PW,118` 八路 hardware-timed PWM architecture；
- ESP32-S3 主控及冻结 GPIO/扩展接口数量；
- 3S LiPo、battery-entry physical fuse、active battery protection和故障覆盖；
- USB POWER source qualification、USB/battery priority、source isolation和反灌要求；
- USB 5 V 调试/降载舵机 source 与 6 V source 的 isolation/selection；
- MASTER_ENABLE、外部常闭 ESTOP及硬件锁存恢复；
- MCU reset、掉电或未武装时 PWM 与舵机电源默认关闭；
- Circuit Design Freeze 前必须清零 Design Input Blocker 和 Component Data Blocker；
- Circuit Design Freeze 首次状态只能是 Proposed / Review Candidate，AI 无权宣布 Accepted/Frozen/Ready for schematic。

## 5. 阶段与证据边界

本修订只改变权威电气设计输入，不完成也不授权新的计算，不授权 schematic、ERC、netlist、PCB layout、采购、下单或实体测试。

下一步仍属于 Circuit Design：完成 `SY8388ARHC` 的 worst-case calculation、器件/无源件选择、pin-by-pin connection、power/fault/default-state tables、layout constraints 和 prototype validation plan；这些内容经 Circuit Design Review、Operator Review及要求的独立人类 review解决后，才能形成并接受 Circuit Design Freeze。
