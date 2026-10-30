# 牛来快跑 PCB Controller 设计输入冻结 v0.2

状态更新：**已由 [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md) 取代；仅作历史记录**

原状态：**Accepted Design Input / Operator 明确修订；尚未开始原理图或网表设计**

日期：2026-08-30

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.1.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.1.md)

## 1. 权威性与合并规则

本文曾作为 PCB Controller Rev A Circuit Design 权威电气设计输入并取代 v0.1；现已由 v0.3 取代。本文保留当时对 v0.1 的规范性纳入和修订，仅用于历史追溯，不得再作为新的 Circuit Design 结论来源。

除第 2、3 节明确修改的项目外，v0.1 的 3S 电池范围、MASTER/ESTOP 安全行为、USB DATA/POWER 边界、器件与 GPIO、外接接口、机械边界和安全要求均保持不变。旧 BOM、Baseline、Decision 和聊天仍不得覆盖本文。

本修订来自 Project maintainer 于 2026-08-30 的明确决定：YSIDO UBEC 不提供制造商资料，其额定能力改为样机验证目标；UBEC 两侧不得采用直接焊线，改用两个带卡扣的 2P 连接器。

## 2. YSIDO UBEC 修订

以下内容取代 v0.1 §2.2 中关于 YSIDO 资料等级和能力保证的对应表述：

| 项目 | 冻结输入 | 状态 / 说明 |
| --- | --- | --- |
| 外置 UBEC 实物 | YSIDO 可调 UBEC，装机固定选择 6.0 V | **Accepted prototype module**；没有 exact MPN 或制造商 datasheet，不得把商城文字当作 manufacturer guarantee |
| 商城宣称 | 输入 7–25.5 V、6 A、最大 8 A；已有页面说明 8 A 不超过 15 s | **Procurement evidence only**；不作为纸面设计已经证明的模块能力、保护行为、热额定或故障响应 |
| UBEC 能力 | 6 A 连续和 8 A 短时 | **Prototype validation target**；必须在最终线束、环境、负载和安全台架条件下验证，验证前不得宣称系统已达到 |
| 主板导电路径 | `VBAT_SWITCHED` 支路及 `UBEC_6V`→`SERVO_V+` 路径仍按 6 A 连续、8 A 短时进行纸面设计 | **Accepted board design target**；主板不应成为已知瓶颈；8 A 波形、温度、压降等仍由未关闭的 Design Input blocker 定义 |
| 未知模块行为 | startup、soft-start、hiccup、reverse-current、反接、过温、短路和 controller-loss 行为均未知 | **Must not assume**；主板必须在不依赖这些未知内部行为的条件下建立默认安全状态，并把不能纸面证明的交互列入 Prototype Validation Risk |

若样机不能达到目标，允许的回退是更换 UBEC 或由 Operator 修订系统工作包络；Circuit Design 不得自行降低已冻结的主板导电路径目标，也不得用固件行为冒充硬件能力证明。

本修订允许仅因缺少 YSIDO manufacturer data 而登记的 `COMPONENT_DATA_BLOCKER` 关闭，因为该模块不再承担纸面保证。它不关闭 8 A 持续/重复周期、环境温度、压降等 `DESIGN_INPUT_BLOCKER`，也不取消样机验证。

## 3. UBEC 两侧连接器修订

以下内容取代 v0.1 §5 的 `UBEC 输入` / `UBEC 电池输出` 机械端接自由度：

| 接口 | 固定针序 | 冻结机械/安全要求 |
| --- | --- | --- |
| 主板至 UBEC 输入 | pin 1=`VBAT_SWITCHED`，pin 2=`GND` | 2P、带卡扣、板端连接器；禁止以直接焊线作为正常装配方案 |
| UBEC 输出至主板 | pin 1=`UBEC_6V`，pin 2=`GND` | 2P、带卡扣、板端连接器；禁止以直接焊线作为正常装配方案 |

两个接口均必须有明确极性和卡扣。优先在成本和供货允许时选择不同系列或不同键位来降低 3S `VBAT_SWITCHED` 与 6 V 回路误插风险；这项机械防误插方式尚未被 Operator 冻结，属于 Circuit Design 的正常工程决策，不得把偏好写成新增系统硬要求。每个连接器及配对端子/线材必须满足最终 6 A continuous / 8 A transient 包络和压降/温升要求；exact MPN、方向和封装由后续选型确定。

## 4. 版本变更摘要

1. YSIDO 从有能力保证含义的 procurement input 改为无制造商资料的 prototype module。
2. YSIDO 的 6 A continuous / 8 A short capability 改为样机验证目标；主板电流路径的纸面设计目标不变。
3. 禁止 UBEC 输入/输出正常装配采用直接焊线。
4. 冻结两个带卡扣的 2P 板端连接接口；exact MPN 及防误插实现留给 Circuit Design。

本版本不授权 schematic、ERC、netlist、PCB layout、采购或实体测试。Circuit Design Freeze 首版仍只能是 Proposed / Review Candidate，且必须等待全部 `DESIGN_INPUT_BLOCKER` 与 `COMPONENT_DATA_BLOCKER` 清零。
