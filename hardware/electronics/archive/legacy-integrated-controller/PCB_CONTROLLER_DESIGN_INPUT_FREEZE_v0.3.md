# 牛来快跑 PCB Controller 设计输入冻结 v0.3

状态：**Accepted Design Input / Operator 明确修订；尚未开始原理图或网表设计**

日期：2026-08-30

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.2.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.2.md)

## 1. 权威性与合并规则

本文是 PCB Controller Rev A Circuit Design 当前唯一权威电气设计输入，并完整取代 v0.2。为避免复制未变化要求造成文本漂移，本文规范性纳入 v0.2 的全部条款；v0.2 又规范性纳入 v0.1 的未变条款。以下修订具有最高优先级，发生冲突时以本文为准。v0.1、v0.2 自本版本接受起仅作为历史快照，不再单独具有设计输入效力。

本修订来自 Project maintainer 于 2026-08-30 的明确决定：接受 DIB-001 至 DIB-008 的推荐值；DIB-009 采用 ESTOP 后必须通过 `MASTER_ENABLE OFF→ON` 才能恢复，并授权修订 Design Input Freeze。

旧 BOM、Baseline、Decision、商城资料和聊天不得覆盖本文。以下数值是主板设计和后续样机验证的系统边界，不代表 YSIDO UBEC、舵机或任何未验证实物已经达到这些指标。

## 2. 电流、温度、压降与逻辑电源包络

| 项目 | Accepted requirement | 说明 |
| --- | --- | --- |
| 舵机路径连续电流 | `6.0 A continuous` | 按热稳态、无限持续时间进行纸面分析；YSIDO 实物能力仍为 prototype validation target |
| 舵机路径瞬态电流 | `8.0 A`，单次最长 `1.0 s`；相邻事件起点至少间隔 `10 s` | 等效长期占空比不高于 10%；用于保险协调、FET SOA、连接器/铜热容量和样机波形 |
| 最大局部环境温度 | `TA_MAX = 40 °C` | 样机在封闭机身最差预期 airflow 下验证 |
| 高电流路径允许温升 | 连续 6 A 时 connector、pad、copper 相对局部环境 `ΔT ≤ 30 °C` | 可触达表面不得仅凭此条宣称安全，仍须结合整机机械边界验证 |
| 半导体结温设计边界 | 正常稳态 `TJ ≤ min(100 °C, TJ_MAX − 25 °C)` | 瞬态不得超过器件 guaranteed operating range |
| UBEC 舵机板内压降 | 从板端 `UBEC_6V` 测试点到最不利 `SERVO_V+` connector pin：6 A steady `≤0.20 V`；8 A transient `≤0.30 V` | 外置 UBEC 与外部线束 regulation/drop 另计并进入样机验证 |
| USB 舵机板内压降 | 从受保护 USB 5 V 舵机源到最不利 `SERVO_V+` connector pin，在 `≤3 A` 时 `≤0.20 V` | USB 仅是调试/降载舵机源 |
| `3V3` 标称值 | `3.40 V nominal`，网名仍为 `3V3` | 选择单一共轨，满足 ESP32-S3 与冻结的 WS2812C-2020-V6 guaranteed supply overlap |
| `3V3` 允许窗口 | 所有正常稳态、允许负载变化及 source switching transient，在最不利负载点保持 `3.30–3.50 V`；ripple `≤50 mVpp` | 超出窗口必须确定性 reset/shutdown，不得以“MCU 可能仍运行”代替合格 |
| `3V3` regulator 能力 | `≥2.5 A continuous`；`3.0 A transient for ≥10 ms` | 受保护外设接口总预算仍为 v0.1 冻结的 0.5 A；屏幕/audio 实际峰值保留样机验证 |

## 3. 电池故障保护与 UBEC 输入支路

### 3.1 不可绕过的实体保险

电池正端进入 PCB 后、任何可形成大电流的支路之前，必须串联一只不可由 firmware 或 active controller 绕过的实体 fast-acting fuse。拓扑顺序固定为：

`Battery connector → physical fuse → active battery protection → protected branches`

实体保险必须满足：

- rated voltage `≥32 VDC`；
- DC interrupt rating `≥100 A`；
- exact current rating、package、time-current curve 与 `I²t` 由 Circuit Design 在本版本电流/温度包络下完成协调；10 A 级仅是初始候选，不是已冻结 MPN；
- active protection 仍必须承担低倍过流、UV/OV、反接和可诊断关断；实体保险是 catastrophic fault backup，不能替代 active protection。

该要求用于覆盖 active-controller loss of power、firmware failure，以及 active protection power MOSFET short 等不能由控制器自身可靠清除的故障。电池真实短路电流未知，仍列为样机/采购输入风险，不得把标签倍率换算值当作 guaranteed clearing current。

### 3.2 UBEC 电池输入支路包络

在不依赖 YSIDO 未知效率、inrush 或内部保护的条件下，主板 `VBAT_SWITCHED`→UBEC 输入支路按以下 accepted envelope 设计：

- `6.0 A continuous`；
- `7.5 A`，持续/重复波形采用第 2 节的 1.0 s / ≥10 s transient definition；
- startup/inrush `≤10 A for 10 ms`；
- 超过上述包络时，主板 active branch protection 允许限流或确定性关断。

该包络只定义主板支路能力，不承诺 YSIDO UBEC 能持续输出 6 A 或短时输出 8 A。YSIDO 模块本体及最终线束必须按 v0.2 的 prototype validation 要求验证。

## 4. 电源反灌与故障传播

除 Type-C CC/VBUS 检测域外，任何断开或未使能的外部电源端口不得被其他 source 有意供电。必须满足：

- `VBAT`、`VBAT_SWITCHED`、`UBEC_6V`、USB POWER VBUS 和 USB DATA VBUS 之间不得形成 charging path；
- 正常温度下 inactive power pin reverse leakage 的设计目标为 `≤100 µA`；最终 pass limit 不得宽于所选器件 guaranteed maximum；
- 任一 source pin short-to-GND 时，reverse-current blocker 必须阻止其他 source 持续向该短路供能；
- fault isolation 优先于 continued operation，发生 source short 时允许主 rail 或整机确定性关断，不要求另一来源无缝维持运行；
- USB DATA VBUS 仍仅用于检测，绝不作为系统电源，也不得向主机反灌。

## 5. USB POWER 能力分级与优先级

USB POWER 是独立 Type-C sink 电源口。不得在未检测 source-current advertisement 时按 3 A 取电。硬件必须区分 Default、1.5 A 和 3 A，并实现以下状态：

| USB POWER 状态 | 无电池时允许行为 | Battery/USB 同时存在时 |
| --- | --- | --- |
| 未连接或无合格 advertisement | 不启动 USB 主电源路径 | 使用 battery 路径 |
| Default current | 只维持 Type-C 检测域；主逻辑和舵机均不得启动 | 不接管逻辑或舵机供电 |
| 1.5 A | `MASTER_ENABLE=ON` 时允许最小逻辑；硬件禁止 USB→servo；屏幕、audio、`3V3_PROTECTED` expansion 默认关闭 | 合格 USB 优先维持最小逻辑；舵机仍不得由该 USB 档位供电 |
| 3 A | `MASTER_ENABLE=ON` 时允许完整逻辑，以及受总输入限流约束的降载 USB servo mode | 合格 USB 优先于 battery 给逻辑供电，以保留电池；舵机源按隔离/优先拓扑选择 |

补充要求：

- USB POWER 在无电池时必须能够独立启动上述允许的逻辑状态；
- 从 USB 移除或切换到 battery 时，`3V3` 必须持续满足第 2 节窗口，或进入确定性的受控 reset/shutdown；
- `MASTER_ENABLE=OFF` 时，除毫安级 Type-C 检测域外，不得启动主逻辑或舵机；
- input current limit、OVP、short protection、reverse-current blocking、hot-plug、brownout 和 source switching 均必须由 Circuit Design 明确定义，不得依赖 firmware 先启动后补救。

## 6. ESTOP 切断、保持与恢复

外部 ESTOP 仍使用常闭干接点，断线、拔出或按下都等同于 loop open。接受的安全语义如下：

- ESTOP loop open 后，不依赖 MCU 或 firmware，在 `≤10 ms` 内撤销 UBEC 与 USB 两条 servo source enable；
- open pulse `≥1 ms` 必须触发；短于 1 ms 的扰动允许由硬件滤波抑制；
- 触发后必须由硬件保持 servo-off fault 状态；ESTOP 触点重新闭合不得自动恢复舵机电源；
- 恢复必须先使 `MASTER_ENABLE=OFF`，随后在 ESTOP loop 已闭合的条件下执行新的 `MASTER_ENABLE OFF→ON` 操作；
- MASTER 重新开启后，MCU reset、掉电或尚未完成 safe-output initialization / servo arm 条件时，PWM 与舵机电源仍必须默认关闭；
- 不增加独立 re-arm 按钮作为冻结接口。若后续希望自动恢复或增加专用 re-arm，属于安全行为/外部接口变更，必须再次修订 Design Input Freeze。

## 7. DIB 关闭与版本变更摘要

本版本逐项关闭 Circuit Design audit 中的 `DIB-001` 至 `DIB-009`：

1. 冻结 8 A transient 的持续时间和重复边界；
2. 冻结最大环境温度、允许温升和结温 derating；
3. 冻结 UBEC/USB 舵机板内允许压降；
4. 冻结单一 3.40 V nominal `3V3` 共轨、电压窗口、纹波和电流能力；
5. 把 battery-entry physical fuse 明确升级为系统硬要求；
6. 冻结 UBEC 电池输入支路 continuous/transient/inrush 包络；
7. 冻结 reverse-current、source short 和 fault propagation 行为；
8. 冻结 USB POWER 独立逻辑能力、advertisement 分级、source priority 与降载策略；
9. 冻结 ESTOP 硬件保持和 `MASTER_ENABLE OFF→ON` 恢复语义。

本版本不选择 exact fuse、MOSFET、eFuse、power mux、3V3 regulator、current-sense 或 TVS MPN；这些仍是 Circuit Design 正常工程决策。本版本也不授权 schematic、ERC、netlist、PCB layout、采购或实体测试。

所有 Design Input 和 Component Data blocker 清零只允许继续完成 Engineering Design Decisions、worst-case analysis 与 Proposed Circuit Design Freeze 候选；AI 不得据此宣布 Circuit Design 已 Accepted、Approved、Frozen 或 Ready for schematic。
