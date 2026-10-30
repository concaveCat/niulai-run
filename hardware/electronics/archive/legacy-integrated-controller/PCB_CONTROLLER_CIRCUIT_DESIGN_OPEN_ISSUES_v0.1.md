# 牛来快跑 PCB Controller Rev A — Circuit Design Open Issues v0.1

> **SUPERSEDED FOR ACTIVE DESIGN 2026-08-31**：Operator 已用 [`MODULAR_CARRIER_HARDWARE_SPEC_v0.1.md`](MODULAR_CARRIER_HARDWARE_SPEC_v0.1.md) 取代本文件所审计的集成控制板架构。以下 DIB/CDB/EDD/PVR 只作历史追溯；当前 blocker 以新规格第10节和当前归档的 [`MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv`](../../MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv) 的 `BLOCKED_*` 行为准。原 v0.1 interface table 未在当前仓库中找到，不补造历史文件。

状态：**Open / Circuit Design audit；不是 Circuit Design Freeze**

日期：2026-08-30

Owner：Project maintainer

工作分支：`hw/pcb-controller-reva-circuit-design`

唯一权威电气设计输入：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.8.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.8.md)；v0.1..v0.7 已被取代并仅作历史记录。

**v0.8 架构变更说明（2026-08-31）**：本台账以下涉及 raw 3S battery front end、LM74800 EOL、SY8388、USB POWER、servo source mux/isolation/current-sense/regen 的条目均是已取代方案的历史审计，不能用于新原理图。新的活跃工作是收敛 `EXT_6V_IN` DC5.5 barrel（先确认2.1/2.5 mm内径）、低压保护、6V→3V3 buck/supervisor、external ESTOP/PDB interface 和 USB DATA VBUS isolation；在逐项重审前，不得从旧行的 open/selected 状态推导 v0.8 结论。

## 1. 文档用途、边界与完成门槛

本文是 Circuit Design 的活跃问题登记册。它区分事实/观测、假设、工程建议和 Operator 决定；历史推荐值只有在权威 Design Input Freeze 明确纳入后才成为 accepted requirement。DIB-001..012 已由当前 v0.7 及其规范性纳入的历史修订关闭。

本阶段只做纸面电路设计、原厂资料审阅和可复现计算。未授权 schematic capture、ERC、netlist、PCB layout、采购、上电或实体测试。

只有同时满足以下条件，才可另行生成状态为 **Proposed / Review Candidate** 的 `PCB_CONTROLLER_CIRCUIT_DESIGN_FREEZE_v0.1.md`：

1. 本文所有 `DESIGN_INPUT_BLOCKER` 已由 Operator 通过更新后的权威 Design Input Freeze 关闭；
2. 所有 `COMPONENT_DATA_BLOCKER` 已由可靠制造商资料、权威输入接受的替代器件，或权威输入明确接受且不削弱功能的整板qualification/production acceptance规则关闭；
3. 所有 `ENGINEERING_DESIGN_DECISION` 已完成候选比较、worst-case 计算、选择和残余风险记录；
4. 所有关键计算可从 `hardware/electronics/calculations/` 按记录命令复算；
5. 全部 pin assignment、net、connector、default state、fault state 和 traceability 表已做完整性检查。

文件存在不代表 Accepted、Approved、Frozen 或 Ready for schematic。人类 review 和明确批准仍是后续独立门槛。

## 2. 审计方法与权威性处理

### 2.1 审计范围

当前重审范围：`EXT_6V_IN` DC5.5 barrel（内径待确认）低电流逻辑输入、外置 UBEC/台式6V互斥接法、外置 high-current ESTOP/servo PDB接口、3.3 V、USB DATA、ESP32-S3 最小系统与 USB/BOOT/RESET、CH32/74HC244 PWM、TCA9539、ICM-42670-L、HAL49ESO、PDM microphone、MAX98357A、WS2812C、足端开关、全部低压 connector/test point/expansion GPIO。已删除的 raw 3S/USB POWER/板载servo power tree 不在 v0.8 范围。

### 2.2 已解决的文档冲突

以下是**观测到但已由权威层级解决**的冲突，不是恢复旧功能的依据：

| 冲突 | 较低权威文档中的旧内容 | 权威 Freeze 的处理 | Circuit Design 约束 |
| --- | --- | --- | --- |
| 专用控制 UART | `HARDWARE_INTERCONNECT_SPEC_v0.1.md` 仍列 `J_CTRL` | 第 3、5.1 节明确删除专用 UART，并把 GPIO39/40 归入 15 路扩展 | 不设计 `J_CTRL`；只保留 UART0 TX/RX 恢复焊盘 |
| 板载红外 | 宏观互连第 2、5 节仍描述四接收一路发射 | 第 4.1、5.1 节明确删除板载专用红外，外置接收器占通用 3.3 V GPIO | 不设计板载 IR RX/TX；GPIO13 只是外置模块候选 |
| 2S/5 A 电源 | 历史 BOM/Baseline 包含 2S 阈值和 5.5 A `LM73100` 路径 | Freeze 第 1、2.1、2.2、9 节明确取代 | 不复制任何 2S 阈值、旧 OR-ing 或 5 A/5.5 A 结论 |

与权威 Freeze 一致前，宏观互连文档只能作为机械线束历史参考。

### 2.3 初始覆盖审计

| 功能块 | 冻结输入是否足以完成纸面设计 | 当前处理 |
| --- | --- | --- |
| 3S LiPo、XT30、9.0–12.6 V | 是；DIB-005 已关闭 | EDD-001/002/003/004 |
| `MASTER_ENABLE` 与关机状态 | 是；DIB-008/009 已关闭 | EDD-005 |
| 外部常闭 ESTOP | 是；DIB-009 已关闭 | EDD-005 |
| 板载 buck 输入支路 | 是；DIB-001/002/005/006/012 已关闭 | EDD-002/024 |
| 板载6 V、6 A continuous/8 A transient | 是；v0.6固定`SY8388ARHC`且删除外置UBEC | EDD-006/007/008/024；板载buck样机能力为PVR-015 |
| USB POWER 与 source qualification | 是；DIB-004/007/008 已关闭 | EDD-009/010 |
| USB DATA | 是；连接架构和 exact connector/ESD/NPN 已从 Operator 候选收敛 | EDD-011 |
| 逻辑 source mux 与 3.3 V | 是；DIB-002/004/008 已关闭 | EDD-010/012 |
| ESP32-S3 最小系统、BOOT/RESET、GPIO | 是 | EDD-013/014；原厂资料已取得 |
| CH32V003/74HC244 PWM engine 与 TCA9539 | 是；v0.4 已取代 PCA9685 | EDD-014/015；原厂资料与无冲突 pin-mux 已核对 |
| ICM-42670-L | 是 | EDD-016；精确 L 后缀资料已取得 |
| HAL49ESO | 是 | EDD-017；`HAL49E SO` 原厂资料已取得 |
| PDM microphone | 是 | EDD-018；精确资料已取得 |
| MAX98357A | 是 | EDD-019；`MAX98357AEWL+T / C2682619` WLP exact connection 已选择 |
| WS2812C-2020-V6 | 是 | EDD-020；精确 V6 资料已取得 |
| 足端开关 | 是 | EDD-021 |
| connectors/test points/expansion | 部分；speaker connector data 已关闭 | EDD-022/023；其余 exact mechanics pending |

## 3. DESIGN_INPUT_BLOCKER

### DIB-001 — 8 A transient 的时间与重复定义（CLOSED）

- **Missing requirement**：冻结输入只给出 8 A“短时”，没有单次持续时间、事件间最短间隔、累计占空比或测试波形。
- **Why it blocks**：无法协调实体保险丝熔断曲线、eFuse/电子断路器 timer、MOSFET SOA、connector/copper 热容量、bulk capacitor 和 8 A pass criterion；“8 A 峰值额定”本身不能证明任意时长安全。
- **Accepted requirement**：`8.0 A for 1.0 s maximum; ≥10 s between event starts（长期占空比 ≤10%）`。6 A continuous 仍按稳态无限时长分析。
- **Technical basis**：8 A 是冻结的系统目标；持续时间与占空比决定 `I²t`、结温摆幅和保险协调。建议波形是可测试且能覆盖启动/短暂停滞的工程边界，但不是 MG90S 或 YSIDO 的制造商保证。
- **Sensitivity**：从 1 s 增至 5 s 会使单次 `I²t` 从 64 A²s 增至 320 A²s，并显著收紧保险、FET SOA 和温升；仅定义毫秒级峰值则会低估短时堵转风险。
- **Alternatives**：A) 8 A/100 ms、低能量启动尖峰；B) 8 A/1 s、10% duty；C) 将主板导电路径按 8 A continuous 设计，但 UBEC 仍需独立定义其 8 A 能力。
- **Recommendation**：接受 B；若 Operator 要求更长，必须同时给出 repetition/duty。仅把 PCB 铜按 8 A continuous 设计不能替代 UBEC 与 fuse clearing 的时间要求。
- **Closure / status**：Operator 于 2026-08-30 接受上述边界；已写入 Design Input Freeze v0.3 §2。**CLOSED**。

### DIB-002 — 最大环境温度与允许温升（CLOSED）

- **Missing requirement**：没有机身内部最大环境温度、连续 6 A 时允许的 connector/copper/器件温升、可触达表面限制或 semiconductor junction derating。
- **Why it blocks**：无法完成 6 A continuous 的热证明，也无法确定 MOSFET、eFuse、buck、电感、保险丝、连接器和铜宽在 75 × 60 mm 板上的 derating。
- **Accepted requirement**：最大局部环境 `TA_MAX = 40 °C`；连续 6 A 时高电流 connector/pad/copper 相对环境温升 `ΔT ≤ 30 °C`；正常稳态半导体 `TJ ≤ min(100 °C, TJ_MAX − 25 °C)`；瞬态不得超过器件 guaranteed operating range 或材料额定值。
- **Technical basis**：冻结 MCU `ESP32-S3-WROOM-1-N16R8` 的官方模块环境等级为 −40…65 °C；必须给电源区留下明确余量。40 °C/30 °C rise 是本项目的保守建议，不是从冻结输入推导出的事实。
- **Sensitivity**：若 `TA_MAX` 改为 50 °C，允许损耗和铜/封装热余量明显下降；若允许温升从 30 °C 放宽到 50 °C，会提高连接器、焊点和邻近器件温度并改变外壳安全评估。
- **Alternatives**：A) 40 °C/30 °C rise；B) 50 °C/20 °C rise；C) 给出强制通风或散热器边界。当前项目没有冻结风扇或散热器，不能默认为存在。
- **Recommendation**：接受 A，并要求样机在封闭机身 worst-case airflow 下验证。
- **Closure / status**：Operator 于 2026-08-30 接受 A；已写入 Design Input Freeze v0.3 §2。**CLOSED**。

### DIB-003 — `SERVO_V+` 允许压降（CLOSED）

- **Missing requirement**：没有定义从 `J_UBEC` 6.0 V 输入到最不利舵机连接器的最大板内压降，也未定义 USB 5 V 路径的压降边界。
- **Why it blocks**：候选 FET、shunt、eFuse、connector、铜宽和 current-sense gain 都依赖可分配的 milliohm budget；没有压降要求无法判断拓扑是否满足“6 V 舵机母线”。
- **Accepted requirement**：UBEC 路径从板端 `UBEC_6V` 测试点到最远 `SERVO_V+` connector pin：6 A steady `≤0.20 V`，8 A transient `≤0.30 V`；USB 路径同一段 3 A 以内 `≤0.20 V`。外部 UBEC/线束的 regulation/drop 另计。
- **Technical basis**：对应板内总电阻预算分别为 `R ≤ 0.20 V / 6 A = 33.3 mΩ` 和 `R ≤ 0.30 V / 8 A = 37.5 mΩ`，可在外 2 oz、低阻 FET 和合格 connector 下分析验证。
- **Sensitivity**：若只允许 0.10 V，器件/连接器/铜面积和成本显著增加；若允许 0.5 V，6 V 源在板端即可降到 5.5 V，削弱冻结 6.0 V 供电意图且增加多舵机负载不一致。
- **Alternatives**：A) 上述 0.20/0.30 V；B) 用百分比（例如 ≤5%）；C) 只规定舵机端最低电压，但需要同时冻结 UBEC regulation 与线束压降。
- **Recommendation**：接受 A，后续另将 harness drop 作为 Prototype Validation Risk。
- **Closure / status**：Operator 于 2026-08-30 接受 A；已写入 Design Input Freeze v0.3 §2。**CLOSED**。

### DIB-004 — 3.3 V 稳态、瞬态与负载边界（CLOSED）

- **Missing/conflicting requirement**：冻结输入只要求“3.3 V”regulator 和 0.5 A 屏幕预算，没有点负载电压窗口、纹波/瞬态下限、总 continuous/peak rail current 或 brownout 判据。新增原厂资料观测：冻结的 `WS2812C-2020-V6` 给出的 VDD 范围下限是 3.3 V，而 ESP32-S3 上限是 3.6 V；因此两者共轨的 guaranteed steady overlap 只有 `3.3–3.6 V`，不能用此前草案的 `3.135 V` 下限宣称满足。
- **Why it blocks**：无法选定 regulator、电感、电容、source mux、USB 模式 load shedding，也无法写出 ESP32、audio、LED 和 expansion 同时变化时的 pass criterion；若 rail 可低于 3.3 V，则 exact frozen LED 的正常功能没有原厂保证。
- **Accepted requirement**：保持单一共轨并把 `3V3` 定义为 `3.40 V nominal`，所有正常稳态/允许负载与 source-switch transient 在最不利点保持 `3.30–3.50 V`，ripple `≤50 mVpp`；regulator 能力按 `≥2.5 A continuous, 3.0 A transient for ≥10 ms` 设计，其中受保护外设接口总预算仍为冻结的 0.5 A。网名仍为 `3V3`。
- **Technical basis**：ESP32-S3-WROOM-1 官方工作电源范围是 3.0–3.6 V；Worldsemi V6 的 VDD 范围是 3.3–5.3 V。可复算脚本得到交集 `3.300–3.600 V`，同轨直接驱动的高/低逻辑 margin 分别至少 0.825 V/0.340 V。2.5/3.0 A 是待 Operator 接受的系统负载包络，不是历史 1.86 A 预算的自动延续。
- **Sensitivity**：若 full-feature steady load 超过 2.5 A，buck、USB input budget 和热设计全部重算；若 rail 瞬态低于 3.3 V，MCU 可继续工作到 3.0 V，但 frozen LED 行为无保证；若 3.40 V nominal 对外部“3.3 V”设备不可接受，则必须分 rail 或改 exact LED。
- **Alternatives**：A) 上述 3.40 V 共轨（推荐，最少器件）；B) 保持 3.30 V nominal 逻辑 rail，给 LED 增加独立 ≥3.3 V rail 和必要 level translation；C) 修订 Design Input，换用保证低于 3.3 V 工作的 exact LED；D) 明确接受 rail 低于 3.3 V 时 LED reset/flicker，但这改变可见行为且仍不能把器件写成 guaranteed operation。
- **Recommendation**：由 Operator 在 A/B/C/D 中明确选择；当前推荐 A。屏幕和 audio 的实际峰值仍列为样机风险。此前 `3.30 V nominal ±5%` 建议已由原厂 V6 数据否定并撤回。
- **Closure / status**：Operator 于 2026-08-30 接受 A；已写入 Design Input Freeze v0.3 §2。**CLOSED**。屏幕/audio 峰值仍是 PVR，不重新打开本项。

### DIB-005 — 不可绕过的实体保险与故障覆盖（CLOSED）

- **Missing/conflicting requirement**：Freeze 要求“电池入口保险或等效不可绕过的故障电流限制”，但没有说明是否必须存在不可复位的实体熔断器、必须覆盖哪些 single-point failures、最大切断时间或允许故障能量。当前任务又要求优先比较 `Battery → physical fuse → active power protection`，并规定若把实体保险升级为硬要求必须由 Operator 决定。
- **Why it blocks**：active protection 的 MOSFET short、controller loss of power 或 gate fault 可能失去限流能力；反之实体 fuse 对低倍过流可能很慢。没有覆盖定义，不能宣称满足 catastrophic PCB short、FET short 和 controller failure。
- **Accepted requirement**：电池正端进入 PCB 后、任何可形成大电流的支路之前必须串联一只不可由 firmware/active controller 绕过的实体 fast-acting fuse；额定电压 `≥32 VDC`、interrupt rating `≥100 A DC`。初始候选为 10 A 级，最终额定值在已冻结电流包络下与 fuse `I²t` 协调后选择。active protection 作为附加层，负责低倍过流、UV/OV、反接和可诊断关断。
- **Technical basis**：650 mAh × 60 C 的标签换算为 `39 A`，但这只是采购输入推算，不是已验证短路电流。Bel C1H 10 A 类器件有 32 VDC/150 A DC interrupt rating；其 guaranteed opening point为 350% rating 时 ≤5 s，说明 fuse 只能作为 catastrophic backup，不能替代 active protection。
- **Sensitivity**：较低 fuse rating 可能被 8 A 脉冲或温度 derating 误熔断；较高 rating 会增加下游短路 `I²t`。没有准确电池内阻时不能把 39 A 当 guaranteed clearing current。
- **Alternatives**：A) physical fuse + active protection（推荐）；B) 单一 eFuse/保护控制器；C) 电池线束内保险。B 无法独立覆盖 power FET short；C 改变外部线束接口且当前未冻结。
- **Recommendation**：由 Operator 明确接受 A 作为系统硬要求，并要求 physical fuse 位于 PCB 电池入口第一保护位置；之后再完成 exact MPN/额定值计算。
- **Closure / status**：Operator 于 2026-08-30 接受 A；已写入 Design Input Freeze v0.3 §3.1。**CLOSED**。exact fuse MPN/额定值仍是 EDD-001，不是新的 Design Input blocker。

### DIB-006 — UBEC 电池输入支路最大电流（CLOSED）

- **Missing requirement**：冻结了 UBEC 6 V/6 A continuous/8 A max 输出标签，但没有可靠效率下限、input inrush、输入电容、current-limit 行为或 PCB `VBAT_SWITCHED` 支路电流上限。
- **Why it blocks**：无法协调主 fuse、battery protection、UBEC branch eFuse/current limit、connector/线材和 9.0 V 低压端的 thermal/SOA。
- **Accepted requirement**：在拿不到可靠 UBEC 资料时，主板设计输入包络为 `6.0 A continuous`、`7.5 A for the DIB-001 transient waveform`、startup/inrush 不得超过 `10 A for 10 ms`；超过即允许 active protection 限流/关断。该包络不承诺 UBEC 本体能输出 6 A/8 A，只定义主板支路能力。
- **Technical basis**：按 `IIN = VOUT × IOUT / (VIN × η)`，在 `VIN=9.0 V`、假设效率下限 `η=0.80` 时，6 A 输出对应 5.0 A 输入，8 A 输出对应 6.67 A 输入；7.5 A 支路给出约 12.5% 余量。80% 仍是待接受的 bounding assumption，不是 YSIDO 数据。
- **Sensitivity**：若效率只有 70%，8 A 输出需要约 7.62 A input；若 startup 超过 10 A/10 ms，branch eFuse/soft-start 可能反复触发。
- **Alternatives**：A) 接受上述保守包络；B) Operator 提供 exact UBEC SKU 和制造商 curves 后据实冻结；C) 换用有可追溯资料的 UBEC。
- **Recommendation**：接受 A 以定义主板输入支路包络。v0.2 已把 UBEC 本体能力改为 prototype validation target 并关闭 CDB-001，因此不再要求通过 B/C 取得 YSIDO 资料；若不接受 A，Operator 需给出另一组支路电流/浪涌包络。
- **Closure / status**：Operator 于 2026-08-30 接受 A；已写入 Design Input Freeze v0.3 §3.2。**CLOSED**。

### DIB-007 — battery/UBEC/USB 反灌和端口故障行为（CLOSED）

- **Missing requirement**：Freeze 禁止 USB 5 V 与 UBEC 6 V 硬并联并要求 reverse-current blocking，但没有量化 inactive connector 允许电压/泄漏，也没有定义 USB/UBEC source short、反接、掉电、输出被外部强拉时另一来源应继续供电还是共同关断。
- **Why it blocks**：不同 ideal-diode、back-to-back FET 和 power-mux topology 在 reverse leakage、short-to-ground isolation、source switchover 和 fault propagation 上不同；这是外部接口与安全行为，不能由器件偶然行为决定。
- **Accepted requirement**：除 Type-C CC/VBUS 检测域外，任何已断开/未使能的外部电源端口不得被其他 source 有意供电；`VBAT`、`VBAT_SWITCHED`、`UBEC_6V`、USB POWER VBUS、USB DATA VBUS 之间均不得形成 charging path。正常温度下 inactive power pin reverse leakage 设计目标 `≤100 µA`。任一 source pin short-to-GND 时，reverse blocker 必须防止另一 source 持续向该短路供能；允许主 rail 关断而不是维持无缝运行。
- **Technical basis**：该 fail-safe 策略优先隔离故障，不把“不断电”置于短路安全之上；USB DATA 已由 Freeze 明确只检测、不供电。
- **Sensitivity**：若要求 faulted source short 时系统无缝继续运行，需要额外 fault isolation、timing 和 SOA；若允许 mA 级 backfeed，外部 UBEC/USB source 的未定义关断行为会进入系统边界。
- **Alternatives**：A) fault isolation 优先、必要时整 rail 掉电（推荐）；B) fault-tolerant continued operation；C) 仅防稳态 reverse current，不覆盖 shorted source。
- **Recommendation**：接受 A 和 100 µA 设计目标；样机测量 pass threshold可根据所选器件 guaranteed maximum 收紧，不能用 typical leakage 宣称通过。
- **Closure / status**：Operator 于 2026-08-30 接受 A 和 100 µA 目标；已写入 Design Input Freeze v0.3 §4。**CLOSED**。

### DIB-008 — USB POWER 独立逻辑、Type-C advertisement 与 source priority（CLOSED）

- **Missing requirement**：Freeze 接受 USB POWER 作为 5 V 调试/降载舵机来源，却没有确认无电池时是否必须启动逻辑；也没有规定 Default/1.5 A/3 A advertisement 各自允许的 rail、battery/USB 逻辑优先级和 hot-plug/brownout 可接受行为。
- **Why it blocks**：无逻辑就无法控制 USB 供电的舵机；而按 3 A 直接取电违反当前任务限制。它决定 Type-C sink/controller、硬件 current qualification、logic mux、load shedding 和源切换 topology。
- **Accepted requirement**：
  - USB POWER 在无电池且 `MASTER_ENABLE=ON` 时可以独立维持最小逻辑；
  - `Default`：只维持 Type-C 检测域，主逻辑和舵机都不启动；
  - `1.5 A`：允许最小逻辑，硬件禁止 USB→servo，屏幕/audio/3V3 expansion 默认关闭；
  - `3 A`：允许完整逻辑和受总输入限流约束的降载 USB servo mode；
  - 合格 USB 优先于 battery 供逻辑，以保留电池；拔出后转 battery，切换瞬态仍须满足 DIB-004；
  - `MASTER_ENABLE=OFF` 时除毫安级检测域外均不得启动。
- **Technical basis**：TUSB320LI 可把 UFP 检测到的 Default/1.5 A/3 A 状态输出给硬件；官方资料指出 Default 对应 USB 2.0 500 mA 或 USB 3.x 900 mA，不能在无数据协商的 POWER-only port 上假设其足以带本板逻辑峰值。
- **Sensitivity**：允许 1.5 A 带 full logic 会压缩 ESP32/audio/display 峰值余量；只接受 3 A 会降低调试电源兼容性；battery priority 会在 USB 插入时继续耗电但减少 source transfer 次数。
- **Alternatives**：A) 上述三档硬件策略；B) 只有 3 A 时启动任何主 rail；C) USB 永不供逻辑，只在电池逻辑存在时作舵机备用。
- **Recommendation**：接受 A；若成本/面积过高，接受 B，而不是未检测便按 3 A 取电。
- **Closure / status**：Operator 于 2026-08-30 接受 A；已写入 Design Input Freeze v0.3 §5。**CLOSED**。

### DIB-009 — ESTOP 切断时间、故障保持和 MASTER-cycle 恢复（CLOSED）

- **Missing requirement**：Freeze 规定 NC contact 断线/拔出/按下都关闭两条舵机路径，但没有最大 shutdown latency、是否 latch、触点重新闭合后是否自动恢复、bounce 或短暂断线行为。
- **Why it blocks**：组合逻辑、hardware latch、power-switch enable、放电路径和 re-arm 接口会完全不同；自动恢复可能造成非预期运动，是安全行为变更。
- **Accepted requirement**：ESTOP loop 打开后不依赖 MCU，在 `≤10 ms` 内撤销全部 servo source enable；故障状态硬件保持，触点重新闭合不得自动恢复；open pulse `≥1 ms` 必须触发，短于该值允许由硬件滤波。恢复必须先使 `MASTER_ENABLE=OFF`，再在 ESTOP closed 条件下执行新的 `MASTER_ENABLE OFF→ON`；MCU 未完成 safe-output initialization / servo-arm 条件时 PWM 和舵机电源仍默认关闭。
- **Technical basis**：断线安全需要 NC loop；重新闭合不自动启动可避免接点回弹或插头重插导致突然动作。10 ms 是待接受的系统要求，不是从现有元件推导。
- **Sensitivity**：更短 latency 增加 noise immunity 难度；允许自动恢复可简化逻辑但增加突然运动风险；把 latch 只放 firmware 会违反“不依赖 MCU/固件”的切断要求。
- **Alternatives**：A) hardware fault latch + explicit re-arm（推荐）；B) open 时硬切、close 时自动恢复；C) 必须 cycle MASTER 才清除。
- **Recommendation**：Operator 选择 C（必须 cycle MASTER 清除），不增加独立 re-arm 按钮。
- **Closure / status**：Operator 于 2026-08-30 明确选择 MASTER `OFF→ON` 恢复；已写入 Design Input Freeze v0.3 §6。**CLOSED**。

### DIB-010 — 用 CH32V003F4P6 + 74HC244 替代冻结的 PCA9685（CLOSED）

- **Missing/conflicting requirement**：权威 Design Input Freeze v0.1 §4 明确接受 exact `PCA9685PW,118`；Operator 现提出以 `CH32V003F4P6 + 74HC244` 作为首选，由 CH32 的 I²C slave 接收命令并使用 TIM1/TIM2 产生 8 路 PWM，74HC244 `/OE` 仍由 TCA9539 控制。聊天提案尚未被权威冻结文件吸收。
- **Why it blocks design**：这不是普通同类 MPN 替代，而是把固定功能 PWM IC 改成带自定义固件的第二 MCU，改变了器件架构、编程/恢复接口、启动和故障行为、I²C 协议与验证范围。未决定前不能冻结 PWM 器件、引脚表、测试点或安全默认链。
- **Recommended value/decision**：若成本优先，修订 Design Input 为功能性要求：8 路 hardware-timed servo PWM；I²C 100/400 kHz slave；外部 buffer `/OE` 上拉默认禁用并由 TCA9539 控制；CH32 reset/未编程/看门狗复位时 buffer input/output 必须确定为 low/high-Z；保留可访问的 SWIO/program/reset；冻结最小寄存器协议、PWM frame/range/resolution、atomic update 和 firmware version identification。首选候选为 `CH32V003F4P6 + 74HC244`，最终须通过 exact pin-mux/data audit。
- **Technical basis**：74HC244 的硬件三态隔离可保留当前 PCA OE 安全层；低成本 MCU 可生成 8 路定时 PWM。但必须证明 F4P6 封装能同时无冲突引出 TIM1 CH1–4、TIM2 CH1–4、I²C、SWIO/reset，并证明 I²C slave、boot GPIO、watchdog/reset 和 3.30–3.50 V 电气行为。自定义固件失效不能绕过 ESTOP 的独立舵机电源切断。
- **Sensitivity**：若 8 路 pin mux 有冲突，需要换封装/MCU或回到 PCA9685；若 74HC244 两个 `/OE` 或输入在启动时未被确定约束，可能在错误时刻输出脉冲；若要求完全仿真 PCA9685 寄存器，会增加固件复杂度，定义项目自有的最小寄存器表更简单但需要修改 ESP32 adapter。
- **Alternatives**：A) 保留冻结的 PCA9685；B) 修订为 CH32V003F4P6 + 74HC244；C) 查找更便宜、资料完整的硬件 PWM IC；D) ESP32 直接输出 8 路 PWM（会占用冻结 GPIO且降低隔离性，不推荐）。
- **Recommendation**：有条件推荐 B，因为两块首板可能节省显著成本，同时 74HC244 可保持独立 `/OE` 安全层；不要在 pin-mux、上电默认和固件接口审计前删除 PCA9685。
- **Operator decision / closure**：Operator 于 2026-08-30 明确授权以该方案取代 PCA9685，并提供 `CH32V003F4P6 / C5187096` 与 Nexperia `74HC244PW,118 / C5623` 原厂资料；已写入 [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md)。**CLOSED**。

### DIB-011 — 板载 Silergy 6 V buck 是否取代冻结的外置 YSIDO UBEC（CLOSED）

- **Missing/conflicting requirement**：v0.4 规范性纳入 v0.2 对“YSIDO 可调 UBEC、装机固定 6.0 V”的 Accepted prototype-module architecture，以及两套带卡扣 2P UBEC 输入/输出接口。Operator 新提出 `SY8388ARHC / C5110279` 为正式板载 buck 候选并认为值得进入原理图；所给文件名/目录条目则对应 `SY83088ARHC / C41348528`。把外置 UBEC 改为板载 buck 会改变已冻结 power tree、外部接口、验证责任和失效模式。
- **Why it blocks design**：Circuit Design 可以比较候选，但不能静默删除外置 UBEC、两套连接器或将 `VBAT_SWITCHED→UBEC` 改为板载 `6V_BUCK→SERVO_V+`；更不能在 Circuit Design Freeze 被人类接受前开始 schematic。
- **Recommended decision**：若 Operator 确实要求取代 YSIDO，修订 Design Input 为“板载 6.0 V synchronous buck，6 A continuous / 8 A for 1 s”；同时明确外置 UBEC 接口是删除、DNP fallback，还是保留 selectable fallback。推荐先保留 DNP 外置 fallback，直到板载 buck 样机通过热/动态/EMI 验证，以降低首次板风险。
- **Technical basis**：Silergy 2026 Q1 Selection Guide 同时列出 `SY8388ARHC` 与 `SY83088ARHC`，均为 4–24 V、8 A、600 kHz、20/10 mΩ、QFN2.5×2.5-16；选型目录不足以完成 pin/ILMT/thermal/compensation/layout 设计。板载 36 W continuous / 48 W transient converter 将原本外置模块的热、环路和 EMI 风险移入 PCB。
- **Sensitivity**：若保留外置 UBEC为主方案，仅增加 DNP footprint，冻结接口变化较小但增加面积；若完全删除外置 UBEC，成本/体积更低但 Rev A 首板没有已知 fallback。
- **Alternatives**：A) 修订冻结输入并以板载 buck 为主、DNP 外置 fallback；B) 板载 buck 为主且删除 UBEC 接口；C) 保持 v0.4 外置 UBEC，Silergy 只留后续 Rev 候选。
- **Operator decision / closure**：Operator 于 2026-08-30 更正 exact为 `SY8388ARHC / C5110279`，接受推荐 A，并明确授权先修订、暂不计算。当前权威 [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.5.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.5.md) 固定板载 buck 为默认 source、两套外置 UBEC connector默认DNP且仅作mutually-exclusive fallback。**CLOSED**。

### DIB-012 — 是否删除外置 UBEC fallback/interface（CLOSED）

- **Requirement change**：v0.5曾保留两套默认DNP的外置UBEC connector和mutually-exclusive fallback；删除它们会改变已冻结外部接口、assembly table和首板fallback能力。
- **Why input-level**：这不是普通连接器选型，而是删除冻结的外部电源接口和系统fallback能力，必须由Operator修订Design Input。
- **Recommendation**：删除两套正式UBEC connector/fallback，避免面积、误装、source fighting和额外保护复杂度；只保留正常测量用`SERVO_BUCK_IN`/`BUCK_6V`/`SERVO_V+` test points，不把测试点定义成外部供电接口。
- **Operator decision / closure**：Operator 于2026-08-30明确选择“直接删除”。当前权威 [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md) 删除两套UBEC connector和全部fallback装配选项，板载`SY8388ARHC`成为唯一正常6 V实现。**CLOSED**。

### DIB-013 — 电池入口 TVS 的系统瞬态包络（OPEN）

- **Missing requirement**：权威输入要求 transient clamp，但没有定义电池连接器可能出现的 source voltage、source impedance、pulse width、单次能量、最大重复次数或最小重复间隔。6 A/7.5 A/10 A load-current envelope不能代替TVS浪涌波形。
- **Why it blocks design**：`SMCJ15CA` 的24.4 V/61.5 A/10×1000 µs数据只对规定波形成立；没有系统包络就无法证明TVS重复寿命、30 V HD303的寄生过冲余量或TPS709 30 V recommended-input余量。
- **Recommended value/decision**：由Operator冻结一个板端浪涌qualification envelope；若没有外部load-dump来源，推荐按线束热插拔/断流的有限能量波形定义，而不引入汽车级任意load-dump要求。允许把实际波形确认保留为PVR，但纸面必须有明确设计边界。
- **Technical basis**：TVS峰值功率额定随脉宽和温度变化；PCB/线束寄生电感还会在TVS器件钳位值之外产生局部overshoot。
- **Proposed closure package**：[`drafts/DIB013_BATTERY_TRANSIENT_ENVELOPE_PROPOSAL_v0.1.md`](drafts/DIB013_BATTERY_TRANSIENT_ENVELOPE_PROPOSAL_v0.1.md)提出：XT30正常只接单只3S LiPo、`L_LOOP≤2.0 µH`，入口按`24 V Thevenin / 1 Ω / 10/1000 µs / 10次 / ≥60 s / 40 °C`qualification；battery-entry TVS不承担servo regeneration。Operator已于2026-08-31确认第一项“正常只接单只3S LiPo、不并接其他source”；第2–4项仍待确认，当前不得据此关闭。
- **Sensitivity**：源阻抗降低或脉宽/重复率增加会直接提高TVS能量和温升，可能需要更大封装、更低钳位或提高主MOSFET/LDO耐压。
- **Alternatives**：A) 定义有限的3S线束热插拔波形并保留SMCJ15CA；B) 换更高耐压FET/LDO后放宽clamp；C) 要求汽车load-dump等级，这会明显增加成本/面积且属于能力变更。
- **Recommendation**：A；在Operator修订Design Input前保持 **OPEN / DESIGN_INPUT_BLOCKER**。

## 4. COMPONENT_DATA_BLOCKER

### CDB-001 — YSIDO UBEC 无 exact SKU 和可靠制造商资料

- **Missing data**：只有品牌/商品级 `6 A, max 8 A, 7–25.5 V, 5/6/7.4 V`；缺 exact MPN、revision、效率曲线、6 A/8 A 的温度与时间条件、input/output capacitance、startup/inrush、UVLO/OVP/OCP/OTP、short behavior、reverse-current behavior、输出调节精度与动态响应。
- **Why critical**：这些数据直接进入 DIB-001/002/006、battery fuse/branch eFuse coordination、source OR、bulk capacitance 和 brownout 分析。商城描述不能作为唯一行为依据。
- **Current evidence**：Freeze 仅把它作为 Accepted procurement input，并没有制造商 datasheet；历史文档也明确要求台架验证。Operator 于 2026-08-30 提供 AliExpress listing `1005007332769016` 和“8 A 不超过 15 s”的页面级说明，但仍无 exact MPN/revision、温度或重复周期。`answer.md` 中整理的 soft-start、hiccup、reverse-current buffer、input reverse-polarity protection 没有随附 manufacturer document，不能作为 guaranteed behavior。上述资料只能证明采购商品和市场宣称存在，不能关闭 DIB-001/CDB-001。
- **Closure**：Operator 于 2026-08-30 明确选择原 closure option D，并由 [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.2.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.2.md) 把 YSIDO 能力改为 prototype validation target。Circuit Design 不依赖未知内部保护行为，也不把 6 A/8 A 写成已验证模块保证。
- **Status**：**CLOSED by authoritative Design Input revision**；剩余实物能力和交互风险转入 PVR-002/PVR-014，主板路径仍受 DIB-001/002/003/006 约束。

### CDB-002 — `ZX-MX1.25-2PZZ` 板端扬声器连接器资料

- **Resolved data**：Operator 于 2026-08-30 提供 LCSC item `C7430458` 所附 one-page manufacturer drawing。正文包含 exact `ZX-MX1.25-2PZZ`、MX1.25/2P dimension row、PCB layout、`1 A AC/DC`、`125 V AC/DC`、`20 mΩ` contact resistance、`−25…+85 °C`、PA housing、tinned-brass contact 和 drawing version 2.0。
- **Pin/view convention**：manufacturer drawing 的 PCB-layout view 对 2P 是对称的且未印数字；Circuit Design 明确定义该 view 左孔为 pin 1=`SPK+`、右孔为 pin 2=`SPK-`，symbol/footprint/review 必须保持这一约定。单扬声器极性不影响 BTL 保护要求；两线仍均不得接地。
- **Evidence**：Operator local review copy `C7430458_线对板针座_ZX-MX1.25-2PZZ_规格书_WJ386547.PDF`；SHA-256 `11c9a5c13c109ae58d8f5f8564db1dad85cd9f565ffda8a947908210c40a47a7`；不将第三方托管 PDF 纳入提交，正式记录只保留 document metadata/hash/source item。
- **Status**：**CLOSED**；直焊备用孔仍按 Design Input 保留。

### CDB-003 — `BX-BD12D07WC5 / C49447025` MASTER 开关资料确认

- **Resolved data**：Operator 于 2026-08-30 提供 Bossie（博锡）原厂单页图纸 `BX-BD12D07WCN`，Rev. A，title=`拨动开关系列`，未标 publication date。系列表明确 `L=5.0 mm` 对应 `BX-BD12D07WC5`；图纸给出 8.70 × 4.50 mm body、2.00 mm travel、三只电气脚 2.00 mm pitch/4.00 mm outer span、两只固定脚位置及材料/额定数据。其 schematic 明确中间电气脚是 moving/common contact，position 1/2 分别连接两侧 throw。
- **Pin/view convention**：原厂图没有给三只焊脚印刷数字；因此 Circuit Design 不声称存在 manufacturer pin numbers。以图纸正视三只电气脚的视图，从左到右定义本项目 footprint pin 1=`MASTER_THROW_1`、pin 2=`MASTER_COMMON`、pin 3=`MASTER_THROW_2`；position 1 接通 1–2，position 2 接通 2–3。最终 ON/OFF 朝向由板边放置方向和丝印共同定义，但必须保持这一 symbol/footprint mapping。
- **Electrical/mechanical evidence**：图纸列出 `DC 50 V, 0.3 A`、contact resistance `<100 mΩ`、insulation resistance `>100 MΩ`、withstand `AC 500 V`、life `5000 cycles`；这些额定值足以覆盖低压 MASTER logic contact，后续仍按 DIB-009 设计安全状态而不把开关本身当故障锁存器。
- **Evidence record**：manufacturer=`Bossie (博锡)`；exact MPN=`BX-BD12D07WC5`；document title=`拨动开关系列`；document/part no.=`BX-BD12D07WCN`；revision=`A`；date=`not stated`；source item=`JLC C49447025`；access date=`2026-08-30`；Operator local review copy `C49447025_滑动开关_BX-BD12D07WC5_规格书_BX-BD12D07WCN.PDF`；SHA-256 `4b0c952aaacb9304fb07f43fa03521a98fa42a059b9de437dc2a46eed3de929e`。第三方托管 PDF 不纳入仓库提交。
- **Status**：**CLOSED**；exact footprint pad/hole allowance 属后续正常封装工程，不再是 component-data blocker。

### CDB-004 — Silergy 8 A buck exact MPN 与完整 datasheet（CLOSED）

- **Original missing data**：Operator 最初文字给出 `SY8388ARHC / C5110279`，首个本地文件名给出 `SY83088ARHC / C41348528`；该文件只是 507 页 *Silergy Catalog Q1 2026* selection guide，且正文同时列出两个不同 MPN，不能作为 pin-by-pin design source。
- **Why critical**：这些数据决定 9–12.6 V→6 V、6 A steady/8 A transient 是否可实现，并直接决定电感峰值/饱和电流、输入/输出电容、反馈、保护阈值、结温和 PCB power-stage layout；不能从 selection guide 猜 pinout 或参考元件值。
- **Resolved evidence**：Operator 更正 exact MPN 为 `SY8388ARHC / C5110279`，并提供 local manufacturer document `C5110279_DC-DC电源芯片_SY8388ARHC_规格书_WJ258659.PDF`。正文 title=`Application Note: AN_SY8388A — High Efficiency Fast Response, 8A, 24V Input Synchronous Step Down Regulator`，revision=`0.9`，revision date=`2020-07-24`，manufacturer=`Silergy Corp.`，ordering table exact=`SY8388ARHC / QFN2.5×2.5-16`，access date=`2026-08-30`，SHA-256=`70ac0d0e4caa97add4e7401bffb6afd48206c37a4e73adaa776b99f5f5e727d3`。
- **Resolved key data**：pins 1 BS；2/3/4 IN；5/15/16 LX；6/14/EP GND；7 PG；8 MODE；9 EN；10 ILMT；11 FB；12 BYP；13 VCC。VIN operating 4–24 V/absolute 26 V；8 A output headline；top/bottom RDS(on) typical 20/10 mΩ；600 kHz (510–690 kHz)；soft-start typ1.2/max1.98 ms；ILMT low/floating/high valley minimum 8/12/16 A；top limit typ22 A；θJA=33 °C/W仅在8.5 cm×8.5 cm四层2 oz evaluation board；recommended TJ≤125 °C；maximum duty约75%；datasheet提供L/C/ripple/transient/thermal公式及四层2 oz layout。
- **Status**：**CLOSED**。资料足以开始 Circuit Design calculation和pin table；不代表器件已被选定，也不授权在 Circuit Design Freeze 接受前 schematic capture。效率、RDS(on)、current limit上端和多项保护参数仍含 typical-only 数据，按 worst-case可得性与样机热/动态/EMI验证管理，不重新制造 CDB。

### CDB-005 — LM74800 正常正输入下 OUT→BAT 反灌保证值（CLOSED by v0.7 system-level acceptance）

- **Missing data**：当前候选 `LM74800QDRRRQ1` 资料描述closed-loop regulation可实现zero DC reverse current，但没有给出与本系统同条件（VIN正常正电压、VOUT>VIN、器件disabled/enabled状态明确）的OUT→BAT steady reverse-current guaranteed maximum。
- **Why critical**：冻结输入目标为inactive external power pin reverse leakage≤100 µA；reverse-battery条件下的`I(C)`/`IREV`不能代替正常正输入的output-backfeed证明。
- **Current evidence**：功能描述支持拓扑可行性，但32 µA和112 µA参数的测试条件均不匹配；不能据此写成≤100 µA guaranteed PASS。
- **Closure options**：A) TI/代理取得同条件written maximum；B) 换用明确给出<100 µA guaranteed max的controller；C) Operator修订Design Input，在不改变`≤100 µA`功能限值的前提下采用整板逐板EOL/qualification规则。
- **Closure evidence**：Operator于2026-08-31接受C并授权开始；[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md)要求首批每块PCBA在25/40 °C执行完整EOL矩阵，放行guardband分别为`|I_PORT|+U95≤50/75 µA`，任何状态最终不得超过100 µA；失败隔离，返修后全矩阵重测。执行规范见[`drafts/EDD002_LM74800_LEAKAGE_EOL_v0.1.md`](drafts/EDD002_LM74800_LEAKAGE_EOL_v0.1.md)。
- **Status**：**CLOSED by authoritative system-level acceptance rule**。这不是datasheet-data closure，也不自动接受EDD-002或证明实物合格；高温、状态矩阵、工艺污染和逐板结果转入PVR-017，首批放行前必须PASS。

### 4.1 已关闭的组件资料审计项

| 器件 | 关闭证据 | 结论/仍需工程处理 |
| --- | --- | --- |
| `ESP32-S3-WROOM-1-N16R8` | Espressif module datasheet v1.8 | 精确 variant、pin、3.0–3.6 V、N16R8 环境等级和 antenna land/keepout 可确认；GPIO assignment 仍属 EDD-013/014 |
| `ICM-42670-L` | TDK InvenSense `DS-000431`, Rev. 1.1, 2021-03-18；LCSC 托管原厂正文 | 精确 L 后缀、14-pin LGA、pin table、I2C application circuit 和 decoupling 可确认；不再用 `ICM-42670-P` 猜 L 后缀 |
| `HAL49ESO` | Hallwee `HAL49E 线性霍尔传感器`, v3.2；LCSC 托管原厂正文 | 订购项 `HAL49E SO` 对应 SOT-23-3L；封装注释明确 SO 脚 1=VDD、2=OUT、3=GND。正文通用 pin 图文字与封装注释排列不同，最终 symbol 必须按 SO 专用封装注释并在 review 截图核对 |
| `LMD3526B261-OFA03` | LinkMems `Specification of MEMS Microphone`, No. M196, v1.0；LCSC 托管原厂正文 | 1.62–3.6 V、PDM、1=DATA/2=L-R/3=CLK/4=VDD/5=GND、bottom-port land/reflow 可确认 |
| `WS2812C-2020-V6` | Worldsemi exact V6 datasheet v1.0, 2026-05-21；LCSC 托管原厂正文 | 3.3–5.3 V、5 mA/channel、pin 1=DO/2=GND/3=DI/4=VDD、100 nF local capacitor 可确认；3.3 V 成板 margin 仍为 PVR-009 |
| `ZX-MX1.25-2PZZ` | Operator 提供 LCSC `C7430458` 所附 manufacturer drawing，version 2.0；SHA-256 见 CDB-002 | exact 2P dimensions、PCB layout 和 1 A/125 V ratings 可确认；pin numbering 使用 CDB-002 明确的 Circuit Design convention |
| Bossie `BX-BD12D07WC5` | Operator 提供 JLC `C49447025` 所附 Bossie `BX-BD12D07WCN`, Rev. A manufacturer drawing；SHA-256 见 CDB-003 | `L=5.0 mm` exact variant、mechanical geometry、center common contact 和 position 1/2 switching 可确认；厂家未印 pin number，使用 CDB-003 明确的 Circuit Design convention |
| Amass `XT30UPB30-M.G.* / C431092` | Operator 提供 Amass manufacturer drawing，正文含 exact family row、额定值、尺寸与 PCB mounting drawing；SHA-256 `02372b47...ca5952` | 选择为 battery board connector；vertical THT male，2 × Ø1.90 mm、5.00 mm pitch；exact color suffix 在装配表确认 |
| Amass `XT30U-M.N.B / C19268019` | Operator 提供 Amass D-1542 / 2021V1 manufacturer specification；SHA-256 `50057652...070f4` | 有 series identity/ratings/mechanical outline，但所给页无完整 PCB drill pattern；保留为未选替代，不据此创建 footprint |
| Hanxia `HX KCD11-2P-R / C6917318` | Operator 提供 manufacturer drawing；SHA-256 `51b39af9...12c2b18a66` | 资料确认其为 14 × 9 mm panel snap-in、0.6–1.5 mm panel thickness 的三脚船形开关，不是 PCB-mount；从 MASTER 候选中排除 |

## 5. ENGINEERING_DESIGN_DECISION

下表中的“选择”若为 `PENDING`，表示尚未完成，不是让 Operator 代做普通器件选择。只有“Operator?”引用 DIB 时才需要人类改设计输入。

| ID | 设计事项 | Candidates / comparison | Calculations | Selected option | Rationale / affected nets & components | Residual risk | Operator? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| EDD-001 | 电池 physical fuse | 1206 SMD；板焊 THT；可插拔 holder；active-only 已由 v0.3 排除。Operator 草稿提出陶瓷填砂 5×20 + PCB holder、breaking 目标 ≥250 A | `C355692` manufacturer data 已确认，但仅为历史 1206/150 A 参考；新候选 exact MPN、DC breaking、fast-acting 曲线和 holder 数据尚未提供 | **PARTIAL / TOPOLOGY SELECTED, EXACT MPN PENDING**：`Battery → 5×20 ceramic fuse + PCB holder → active protection` | 可维护性和预期分断能力优于旧 1206；仍需把 holder 接触电阻纳入 6 A/8 A 压降/热计算 | 目标 ≥250 A 尚未升级为 Design Input 硬下限；最坏短路经验边界、振动保持、温升和 no-nuisance 仍属 PVR | No；DIB-005 closed |
| EDD-002 | 电池反接/UV/OV/OC active protection | `LTC4368-2 + back-to-back NFET`；`LM74800QDRRRQ1 + INA300AIDGSR + back-to-back NFET`；LM7480 + separate eFuse | [`verify_battery_uvov_window.py`](calculations/verify_battery_uvov_window.py)；[`verify_lm74800_eol_contract.py`](calculations/verify_lm74800_eol_contract.py)；`drafts/EDD002_BATTERY_PROTECTION_v0.1.md`；`drafts/LM74800_INA300_REVIEW_v0.2.md`；[`drafts/EDD002_LM74800_LEAKAGE_EOL_v0.1.md`](drafts/EDD002_LM74800_LEAKAGE_EOL_v0.1.md) | **PREFERRED CANDIDATE / NOT ACCEPTED**：`LM74800QDRRRQ1 / C3215600`；`INA300AIDGSR / C529558`；`HD303N080SG / C52766420 ×2`；`MCSF2512DR005Q9 / C49437818` 5 mΩ Kelvin；`TPS70933DBVR / C89347` + `BAS116,215 / C48502` AON；`SMCJ15CA / C151273` TVS 首选；初始 `RUV=59.0 kΩ/10.0 kΩ`、`ROV=100 kΩ/10.0 kΩ`、`RLIMIT=2.74 kΩ`。旧 `LTC4368IMS-2#PBF` 保留为比较基线 | 一个控制器完成反接、背靠背驱动、UV/OV、硬件 enable 与理想二极管行为；INA300 以10–12 A级快速过流比较器触发LM74800 EN/UVLO wired-kill；v0.7以首批逐板25/40 °C EOL保持`≤100 µA`功能要求，不等待不存在的整板datasheet maximum | PVR-017逐板反灌结果；DIB-013 TVS系统瞬态envelope；INA300全温VOS/ILIMIT/latch timing、ALERT→EN/UVLO电平接口、TVS/FET 30 V余量、shunt/阻值完整corner仍未闭合。旧LTC4368脚本不能证明新候选 | No；CDB-005 closed by v0.7 system acceptance |
| EDD-003 | `SERVO_BUCK_IN` branch protection after battery protection | direct feed；additional high-current eFuse；duplicate active limiter | battery-protection pass-through, buck input inrush and fault-energy calculations | **RETIRED / SUPERSEDED BY v0.6**：不再有独立 UBEC branch；`VBAT_PROTECTED → SERVO_BUCK_IN` 是唯一正常板载 buck 输入路径 | v0.6 删除外置 UBEC 及其专用支路/连接器；MX74610/LMX5069/TPS259827 不得进入当前网表，除非未来重新修订 Design Input 恢复该功能 | 历史 UBEC 计算仅作参考；仍需验证 SY8388 输入浪涌、短路和电池保护协调 | No；DIB-012 closed |
| EDD-004 | 逻辑 battery branch | low-current eFuse；reverse-blocking load switch；direct from common protection | [`drafts/POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md`](drafts/POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md) defines separate battery logic switch into wide-input 3V3 buck | **TOPOLOGY SELECTED / MPN PENDING**：`VBAT_PROTECTED → BAT_LOGIC_SWITCH → LOGIC_SOURCE`；约2 A input-limit class；USB合格时hardware-off；MASTER OFF/default时off | 与USB logic switch组成break-before-make、USB-priority mux；不要求source transfer无缝，允许supervisor controlled reset | exact reverse-blocking switch/eFuse、off leakage/thermal/short行为待选 | No |
| EDD-005 | MASTER/ESTOP/ARM 硬件逻辑 | discrete async latch；supervisor/CPLD；串联 gate only | [`drafts/EDD005_MASTER_ESTOP_v0.1.md`](drafts/EDD005_MASTER_ESTOP_v0.1.md) + `verify_estop_frontend.py`：NC loop open guaranteed-low 1.073–1.338 ms，close guaranteed-high ≤0.148 ms | **TOPOLOGY SELECTED / EXACT LOGIC MPN PENDING**：AON async fault latch；ESTOP open/POR async-set fault；仅 `MASTER_OFF AND ESTOP_CLOSED` clear；`SERVO_HW_ENABLE=MASTER_ON AND !FAULT_LATCH`；无独立 ARM button | 不依赖 MCU；ESTOP重新闭合不恢复；MASTER OFF≥1 ms→ON恢复；同时硬件撤销 SY8388 与 USB servo-source enable | exact flip-flop/Schmitt/gates/POR/ESD/connector待选；AON brownout/default state必须由器件 guarantees闭合 | No；DIB-008/009 closed |
| EDD-006 | 板载 buck servo source isolation/OR | external back-to-back NFET controller；integrated bidirectional-off load switch | `POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md` fixes source priority/fault states；RDS/thermal待exact MPN | **TOPOLOGY SELECTED / MPN PENDING**：`BUCK_6V → BUCK_SERVO_ISO → SERVO_SOURCE`；battery有效时为servo优先source；6 A continuous/8 A 1 s；ESTOP/default off | 与USB路径hardware interlock、break-before-make；不要求servo rail无缝切换 | exact controller/FET、reverse leakage、drop/SOA和buck output reverse drive待选 | No |
| EDD-007 | USB servo source switch/OR | same external-NFET controller；integrated 3 A reverse-blocking switch | ≤3 A drop/thermal、reverse leakage、faulted-source short | **TOPOLOGY SELECTED / MPN PENDING**：`USB5_PROTECTED → USB_SERVO_ISO → SERVO_SOURCE`；仅无battery且USB advertisement=3 A时允许；Default/1.5 A硬件禁止 | battery有效时始终选择BUCK_6V，避免插USB时6→5 V切换；source transfer先双off再目标on | exact switch、USB总输入限流、transition overshoot待选 | No |
| EDD-024 | 板载 6 V servo buck | exact `SY8388ARHC / C5110279`；旧外置YSIDO方案仅作历史 | [`drafts/EDD024_SY8388_POWER_STAGE_v0.1.md`](drafts/EDD024_SY8388_POWER_STAGE_v0.1.md) + `verify_sy8388_buck.py`：2.7 µH ±20%、8 A peak 9.43 A、CIN RMS bound 4 A、COUT effective minimum screen 269 µF | **ELECTRICAL VALUES SELECTED / EXACT PASSIVE MPN + TOTAL THERMAL PENDING**：PFM default、ILMT low、100 kΩ/11.1 kΩ FB、2.7 µH、CIN effective≥40 µF、COUT effective≥330 µF/ESR≤10 mΩ | worst ripple 2.85 A=35.7%；COUT 330 µF gives 23% capacitive-step margin；完整 pin connection、mandatory BS/VCC/BYP values和layout constraints已记录 | exact inductor/capacitor DC-bias/DCR/ripple/thermal MPNs；QFN功率密度、48 W transient、EMI、regen与PVR-015仍待闭合 | No；DIB-011/012 closed |
| EDD-008 | common servo protection/current sense/clamp | source-specific protection + common shunt/amplifier；Hall current sensor；common eFuse | `POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md`：5 mΩ×gain50 gives 1.50/2.00/3.00 V at6/8/12 A；drop30/40 mV | **ARCHITECTURE SELECTED / MPN PENDING**：各source独立限流/隔离；common 5 mΩ Kelvin + 3.3 V high-side gain50 amplifier→GPIO5 ADC；telemetry不承担安全关断 | 保持低成本和≤0.20/0.30 V drop budget；battery OCP和USB input limit执行安全保护 | exact shunt/amplifier/RC/ADC clamp和6 V regen clamp MPN；单向测量不显示negative current，保留PVR | No；DIB-001/003 closed |
| EDD-009 | USB POWER Type-C front end | TUSB320LI/LAI GPIO decode；fixed Rd + conservative mode；PD sink controller | v0.3 §5 state table已映射到AON硬件；exact controller truth table/CC connection待MPN | **TOPOLOGY + SELECTION SPEC FIXED / EXACT MPN PENDING**：sink-only Type-C current-advertisement controller由USB AON供电，硬件输出Default/1.5/3 A；Default禁止main logic/servo，1.5 A禁止USB servo，3 A才允许USB servo | 不依赖ESP启动后才判断可取电；status同时送source logic和TCA P11/P13 | exact suffix/CC network/standby current/hot-plug state truth table、USB OVP/eFuse待选 | No；DIB-008 closed |
| EDD-010 | USB input protection与 logic source mux | eFuse + two reverse-blocking switches；integrated wide-input mux | [`drafts/POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md`](drafts/POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md)：2.5 A/1 ms/0.2 V seamless hold-up需12.5 mF，故选择controlled reset | **TOPOLOGY SELECTED / MPN PENDING**：USB-priority、break-before-make；Default=AON only；1.5 A=min logic；3 A=full logic；battery path off while USB qualified；source transfer先servo/PWM off并允许supervisor reset | 避免大hold-up电容和不可靠source overlap；所有路径需reverse blocking和short isolation | exact USB eFuse/mux/load switches、transfer timing、off leakage和brownout sequence待选 | No；DIB-004/007/008 closed |
| EDD-011 | USB DATA | 16-pin USB 2.0 Type-C receptacle；独立 Rd；低电容四通道 ESD；VBUS 直接逻辑输入 vs 隔离检测 | 本轮不新增参数计算；只验证 Operator 候选身份/封装/原厂资料 | **CONNECTION + MPN SELECTED**：connector=`SHOU HAN TYPE-C16PIN / C393939`；ESD=`Texas Instruments TPD4E05U06DQAR / C138714`；VBUS detector=`onsemi MMBT3904LT1G / C81464`；CC1/CC2 各 5.1 kΩ ±1% 到 GND；A6/B6 合并为 D+、A7/B7 合并为 D−；四通道 ESD 覆盖 D+/D−/CC1/CC2；D+/D− 各经靠近 ESP 的 22 Ω 串阻到 GPIO20/19；VBUS 只驱动隔离的 active-low presence detect，不进 power tree | `USB_DATA_VBUS_RAW` 经限流电阻驱动 NPN，collector 以 10 kΩ 上拉到 3V3 后进 TCA P10；3V3 掉电时 VBUS 不直接连接任何 3V3 IC pin；connector shell 直接接板 GND 并就近过孔 | connector drawing orientation、SI/ESD 成板结果 | No |
| EDD-012 | 3.3 V regulator | wide-input synchronous buck；buck-boost；separate regulators | `POWER_ARCHITECTURE_WITH_EDD002_ISOLATED_v0.1.md` proves buck-only sufficient because all main sources remain above3.5 V；full load/source calculations wait exact MPN | **TOPOLOGY + SELECTION SPEC FIXED / EXACT MPN PENDING**：single adjustable synchronous buck after mux；VIN target≥4.5–18 V；3.40 V；≥2.5 A continuous/3 A 10 ms；EN default-low、PG preferred | one regulator handles USB5 and battery9–12.6，no buck-boost/dual rail；transfer reset handled by supervisor | exact MPN/L/C/feedback/efficiency/thermal and load-step validation pending | No；DIB-002/004 closed |
| EDD-013 | ESP32 最小系统 | 仅 EN RC；EN RC + voltage supervisor；无 supervisor | 本轮只审阅 Operator 所给原厂资料，不跑 threshold/rail 参数计算 | **CONNECTION + CANDIDATE MPN SELECTED / CALC DEFERRED**：supervisor=`TECH PUBLIC TPS3808G33DBVR-TP / C51886189`；SOT-23 pin 1 RESET_N→ESP EN、2 GND、3 MR_N←RESET button、4 CT、5 SENSE→3V3、6 VDD→3V3，VDD local 100 nF；ESP EN 10 kΩ上拉/1 µF到GND；GPIO0 10 kΩ上拉并用 BOOT 按键接地；GPIO43/44 只到 UART0 recovery TP | exact manufacturer pinout/electrical/package data 与 v0.3 rail window 已确认；threshold/CT compatibility 可计算但按指示暂缓 | local bulk 与 regulator output network 协调；计算完成前不宣布 supervisor fully locked | No |
| EDD-014 | 完整 ESP32 GPIO allocation | 旧映射重做；保持冻结 15 路全部 connector-accessible；GPIO7/8/9 via DNP 0 Ω to Hall2-4 | [`esp32_gpio_assignment.csv`](calculations/esp32_gpio_assignment.csv) + 校验脚本 | **SELECTED**；15 路 expansion、18 路 onboard/recovery、3 路 N16R8 PSRAM reservation 完整闭合 | GPIO4/5/6=ADC，11/12=I2C，19/20=USB，43/44=UART0 recovery，46=PDM_CLK；其余见机读表 | GPIO3/45/46 均须遵守 strap 默认和 3 ms hold；GPIO46 已由原厂资料确认可输出 | No |
| EDD-015 | CH32 PWM engine/TCA9539/74HC244 分工与安全默认 | v0.4 已取代 PCA9685；CH32V003F4P6 + 74HC244PW,118；TCA hardware OE gate | [`drafts/EDD015_CH32_PWM_PROTOCOL_v0.1.md`](drafts/EDD015_CH32_PWM_PROTOCOL_v0.1.md) + `verify_ch32_pwm_protocol.py`：0x40 I²C、1 µs tick、50 Hz、500–2500 µs、atomic commit、100 ms watchdog | **CIRCUIT/FIRMWARE INTERFACE + PIN MAPPING SELECTED**：TIM1 PD2/PA1/PC3/PC4→PWM1–4；TIM2 remap3 PC1/PC7/PD6/PD5→PWM5–8；PC5/PC6 I²C；74HC244 exact pin mapping已定义；两 `/OE` 并联、10 kΩ上拉，经1 kΩ由TCA P15只在安全初始化后拉低；A/Y各100 kΩ下拉 | shadow register 0x08–0x17、0xA5 atomic commit、invalid range reject、watchdog timeout disables timers/pins low；SWIO/NRST和8 PWM TP保留 | firmware implementation/HSI pulse accuracy/two-timer phase≤2 µs仍须验证；power-status polarity随剩余power MPN收敛 | No；DIB-010 closed |
| EDD-016 | ICM-42670-L 接法 | I2C with INT1；SPI；INT2 test point | DS-000431 Rev.1.1 pin/application circuit逐脚审计 | **SELECTED**：3.3 V I2C、地址 0x68、INT1→TCA P16、INT2→TP | pin1 AD0=GND；2/3/10/11 NC；4 INT1；5 VDDIO；6 GND；7 FSYNC=GND；8 VDD；9 INT2 TP；12 CS=VDDIO；13 SCL；14 SDA；VDD 0.1 µF+2.2 µF、VDDIO 10 nF，均 ±10% X7R | 板弯/振动/磁声耦合仍为 PVR-007；共享 I2C pull-up 由 EDD-015 收敛 | No |
| EDD-017 | HAL49ESO | one assembled + three DNP；per-sensor RC；DNP 0 Ω share | [`verify_hall_adc_frontend.py`](calculations/verify_hall_adc_frontend.py)：worst ADC=2.838 V，headroom=62.2 mV，`fc`=67.3–82.2 Hz | **SELECTED**：每路 OUT→27.4 kΩ→ADC node，100 kΩ/100 nF to GND；100 nF local bypass；Hall group 1 µF bulk | SO package pin1=VDD、2=OUT、3=GND；GPIO6 fixed Hall1；Hall2-4 ADC node经独立 default-DNP 0 Ω 到 GPIO7/8/9 | 3.3/3.4 V 实际 zero/sensitivity/field range无完整曲线，保留 PVR-008 | No |
| EDD-018 | PDM microphone | GPIO matrix PDM pins；L/R strap；bottom-port acoustic keepout | GPIO 表完整性；M196 v1.0 pin/voltage/timing audit；logic-margin附件 | **SELECTED**：GPIO46=PDM_CLK 2.4 MHz standard mode，GPIO21=PDM_DATA；L/R 以互斥 0 Ω 选择，默认 GND | mic pin1 DATA→33 Ω（靠 mic）→GPIO21，2 L/R→0 Ω strap，3 CLK←33 Ω（靠 ESP）←GPIO46，4 VDD→3V3，5 GND；本地 100 nF+1 µF；GPIO46 10 kΩ下拉，启动 3 ms 后才输出 clock | 声孔污染、音频耦合与 L/R 采样沿需 firmware/prototype 验证 | No |
| EDD-019 | MAX98357A | `MAX98357AETE+T` TQFN-16 vs `MAX98357AEWL+T` 0.4 mm WLP；3V3 common rail vs 独立 audio rail；filterless vs output filter | 本轮不计算 output power/thermal；Operator 报价为扩展库 WLP、约 ¥3.01/board | **SELECTED：`MAX98357AEWL+T / C2682619` 9-WLP**，3V3、standard I2S；A1 SD_MODE←TCA P14且100 kΩ下拉，A2 VDD→3V3并100 nF+10 µF，A3 OUTP→SPK+，B1 DIN←GPIO18，B2 GAIN_SLOT→相邻 C2 GND（12 dB no-via），B3 OUTN→SPK−，C1 BCLK←GPIO16，C2 GND，C3 LRCLK←GPIO17 | manufacturer datasheet Rev.11 exact ordering/bump table 已核对；BCLK/LRCLK/DIN 各预留靠 ESP 的 33 Ω串阻；最小 filterless BTL；WLP 制造约束进入后续 layout constraint，不为此增加电气器件 | 实际音量/噪声/EMI、0.4 mm WLP 首板贴装/检查 | No |
| EDD-020 | WS2812C chain | direct same-rail drive；level shifter；separate LED rail | logic-margin附件：高/低 margin 0.825/0.340 V；4×3×5 mA=60 mA，3.6 V 时 0.216 W | **SELECTED topology**：GPIO41 direct→33 Ω→LED1 DI，100 kΩ下拉；4 颗串联；每颗 100 nF，chain入口 4.7 µF | same-rail ratio threshold使 level shifter 无必要；pin1 DO、2 GND、3 DI、4 VDD；v0.3 已冻结 3.30 V steady lower limit | EMI/误码与 actual rail noise 仍为 PVR-009 | No；DIB-004 closed |
| EDD-021 | foot switches | TCA inputs with pull-up/RC/ESD；ESP direct inputs；四通道 array vs 四颗单路 ESD | 本轮不做 RC tolerance sweep；只验证候选身份和适用电压 | **CONNECTION + ESD MPN SELECTED**：TCA P00–P03 各以 10 kΩ上拉到3V3、100 nF到GND，经过 1 kΩ connector-side series resistor 到 `SW`；每路外部端点侧使用一颗 `H5VL10B / C7420372` 双向 ESD，共四颗；四组电气端点均固定为 pin/pad 1=`GND`、pin/pad 2=`SW`。按 Operator 决定，不为通用 2P 排针选择 exact MPN，也不把排针作为独立网表器件 | 3.3 V 慢速输入不需要为共料使用低电容高速阵列，5 V VRWM 足够且四颗总成本低；TCA POR/reset 时端口为 input，开路读 high、触点闭合读 low；NO/NC 由接线与 firmware polarity 解释 | 外部端接机械保持力、长线 EMI/contact bounce | No |
| EDD-022 | exact connectors/switches | manufacturer-documented XT30/USB/THT connector families；right-angle maintained MASTER slide switch | 本轮不做 contact/thermal 计算 | **PARTIAL**：battery=`Amass XT30UPB30-M.G.* / C431092`；speaker=`C7430458`；ESTOP generic 2P；MASTER=`Bossie BX-BD12D07WC5 / C49447025`。v0.6明确删除UBEC input/output connector；历史`C8445/C71370`不得进入当前component/net/assembly table。`BX-BD12D07WC7 / C49447027`是长柄替代；`SS-12D02-G040 / C2876023`未选；panel-mount C6917318已排除 | WC5提供板边可操作性且不过度外伸；项目footprint按图纸正视三脚从左到右定义throw1/common/throw2，最终MASTER network connection与ON/OFF丝印在相关power-state decision完成后固定 | switch actuator可达性最终受机械布置验证 | No |
| EDD-023 | test points/temperature monitor/expansion protection | dedicated TPs；power-area NTC；3V3 PTC/load switch；GPIO series/ESD | [`drafts/EDD023_TESTPOINTS_EXPANSION_v0.1.md`](drafts/EDD023_TESTPOINTS_EXPANSION_v0.1.md) gives complete power/safety/recovery/PWM TP set and 15-GPIO protection rules | **TOPOLOGY SELECTED / EXACT PROTECTION MPN PENDING**：no active temperature-monitor IC；DNP NTC footprint near SY8388/inductor；3V3 expansion≤0.5 A via resettable protection；15 expansion GPIO each gets near-MCU 100 Ω series option and connector-side ESD; 3V3-only silkscreen | avoids unnecessary monitor IC while retaining thermal characterization point；no raw battery/buck/USB rail exported；test pads are not injection connectors | exact PTC/ESD arrays/NTC and mechanical TP footprint；probe access only verifiable in PCB review | No |

### 5.1 已关闭或已选 topology 的低压工程决策说明

- `EDD-011/013/019/021` 的第一批嘉立创候选验证见 [`PCB_CONTROLLER_COMPONENT_SELECTION_QUERY_v0.1.md`](PCB_CONTROLLER_COMPONENT_SELECTION_QUERY_v0.1.md)。已收敛 `C393939` Type-C、TI `C138714` USB ESD、`C81464` NPN 和四颗 `C7420372` 足端 ESD；通用足端排针按 Operator 决定不进入 exact electrical/netlist part selection。`MAX98357AEWL+T` WLP 是有效候选，等待其嘉立创 item/装配成本后与 TQFN 做最终封装选择。`TPS3808G33DBVR-TP` 是 TECH PUBLIC 器件而不是 TI 原厂 `TPS3808G33DBVR`，在完成自身资料审计和 DIB-004 rail 确认前不锁定。
- `EDD-011` 的 USB DATA VBUS 不直接进 TCA/ESP，也不进入任何 power tree；active-low transistor detect 保证电脑 VBUS 不依赖未供电 IC 的输入行为。CC1/CC2 各自保持独立 Rd，D+/D−只在 receptacle orientation pins 合并后进入保护和 ESP32。
- `EDD-013` 选择 supervisor + EN RC；v0.3 已冻结 source switching/brownout rail window。RESET 作用于 supervisor manual-reset path，BOOT 只作用于 GPIO0。按 Operator 指示，supervisor threshold、CT 和 local bulk 计算仍暂缓，完成前不锁定。
- `EDD-015` 已完成 TCA/CH32/74HC244 功能分配。安全链为：GPIO3 复位或 MCU reset → TCA 回到 inputs → P15 不再拉低 → 74HC244 OE 外部 pull-up → 全部 servo PWM connector 由各自 100 kΩ保持 low。该链只保证 PWM safe default，不替代 EDD-005 的硬件 servo-power arm/ESTOP。
- `EDD-019` 采用最小 filterless BTL connection，不增加未经验证的 LC 网络；冻结扬声器的线圈电感没有可靠资料，因此实际 output、EMI 和 noise 保留 PVR-006，失败时才按测量结果重做滤波。`AMP_ENABLE` 外部下拉保证 TCA reset/MCU reset 时关断。
- `EDD-021` 的四路足端输入在 TCA reset 时仍是 passive pull-up 的确定状态；硬件 RC 只抑制边沿噪声，最终 debounce 和 NO/NC polarity 均由 firmware 完成，不把足端开关加入安全 ESTOP 链。
- `EDD-014` 的权威机读映射位于 [`calculations/esp32_gpio_assignment.csv`](calculations/esp32_gpio_assignment.csv)，复核命令为 `python3 hardware/electronics/calculations/verify_esp32_gpio_assignment.py`。2026-08-30 结果为 PASS：36 个 module-exposed GPIO 行、3 个 N16R8 Octal-PSRAM 保留脚、33 个 board-usable GPIO、15 个冻结 expansion GPIO 和 18 个 onboard/recovery GPIO 全部唯一且无遗漏。
- Espressif *ESP32-S3 Series Datasheet* v2.2 把 GPIO46 列为 `I/O/T`，并说明 GPIO0/3/45/46 的 strap 在 `CHIP_PU` 上升后至少保持 3 ms，之后释放作普通 GPIO。因此历史映射中的 GPIO46→PDM_CLK 可以保留；此前“GPIO46 input-only”的初步判断已被原厂资料更正，不作为设计结论。
- `EDD-016` 仅依赖精确 `ICM-42670-L` 的 DS-000431 Rev.1.1，不再借用 `-P` 后缀资料。四个 `RESV` 脚选择 NC 是原厂明确允许项；I²C 总线 pull-up 属于 shared-bus 设计，不重复放在 IMU 器件旁。
- `EDD-018` 保留 GPIO46 外部 10 kΩ 下拉；麦克风 CLK 为输入，因此复位期间不会反向改变 boot strap。PDM 时钟固定为 2.4 MHz standard mode，满足 M196 的 1.0–4.8 MHz/45–55% duty 范围；firmware 必须配置并验证该频率。
- `EDD-017` 使用 27.4 kΩ/100 kΩ divider 和 100 nF ADC capacitor；最不利 3.6 V Hall output 被限制到 2.838 V。由于原厂只在 5 V 给出完整 transfer 参数，3.3/3.4 V 实际传递函数继续按 PVR-008 测试，不猜测。
- `EDD-020` 选择 same-rail direct drive；v0.3 采用 3.40 V nominal / 3.30–3.50 V window，关闭了 exact V6 供电下限与旧 3.135 V 草案的冲突。

## 6. PROTOTYPE_VALIDATION_RISK

这些事项允许保留到 Proposed Circuit Design Freeze，但必须作为样机验证门槛，且当前阶段不执行实体测试。

| ID | Risk / predicted behavior | Test method and condition | Measurement point | Pass criterion | Fail criterion | Fallback / redesign |
| --- | --- | --- | --- | --- | --- | --- |
| PVR-001 | 6 A continuous path 应在冻结热边界内；8 A 按最终波形不触发误保护 | 封闭机身、最终线束，40 °C chamber/等效预热；6 A 到热稳态，随后 8 A 波形重复 | 每个 power IC/FET/fuse/shunt/connector、铜面、`SERVO_V+` | 满足 DIB-002/003 最终值；无 reset/oscillation | 任一点超温/超压降/保护误动作 | 增铜/并联触点/低阻 FET/更大封装或重构 power path |
| PVR-002 | 八舵机启动/堵转可能超出6/8 A包络或触发SY8388 hiccup/current limit | 机械固定、运动限位、人员净空、可触达ESTOP；逐只→组合启动，短时受控堵转 | `SERVO_BUCK_IN`、`BUCK_6V`、`SERVO_V+` current/voltage、temperature | 峰值和时间落入DIB-001/006；无逻辑误复位 | 超包络、buck振荡/hiccup、线束/connector过热 | firmware stagger/限并发、调整buck L/C/ILMT；仍失败则需更换buck并重算 |
| PVR-003 | battery/USB hot-plug 切换可能产生 3V3 droop/overshoot | 各 source 单独/同时，在 min/max load 下插拔；重复 source fault/remove | `LOGIC_SOURCE`, `3V3`, EN/reset, source status | 全程满足 DIB-004 或按冻结表确定性 reset；无 backfeed | 随机 reset、overshoot、source chatter | 调整 priority/hysteresis/soft-start/hold-up cap或换 mux |
| PVR-004 | 3V3 full-feature peak 可能高于纸面预算 | Wi-Fi/BLE TX + audio tone + four LEDs + display/expansion load step | buck input/output、电感、ESP3V3、最远 3V3 pin | 电压/温度满足最终 rail spec | brownout、限流或热超限 | load shedding、提高 regulator、audio改5V专支路、降低 expansion budget |
| PVR-005 | USB DATA SI/ESD/双口串扰只能在成板确认 | USB enumeration/throughput/reconnect、ESD pre-compliance、POWER port hot-plug noise | D+/D− near connector/MCU、VBUS sense、3V3 | 稳定枚举/恢复，无异常 reset；眼图目标后续定义 | 丢枚举、CRC/断连、ESD锁死 | 调整串阻/ESD/stackup/routing/屏蔽连接 |
| PVR-006 | Class-D audio、buck、servos会耦合到 microphone/IMU/USB | 静音/满幅 tone、servo step、RF TX 组合；录 raw PDM/noise spectrum | mic data、3V3、SPK±、IMU noise | 无不可接受底噪/自激；量化门槛在实验计划冻结 | 啸叫、严重底噪、IMU spur | 分区/滤波/供电隔离/声学隔离/降增益 |
| PVR-007 | IMU offset 受板弯、回流、振动和电感/扬声器影响 | 静态六面、servo/audio on/off、重复温度点 | raw accel/gyro/temp | self-test通过；offset/noise落入后续实验窗口 | 饱和、明显耦合或漂移 | 移位/加强板/滤波/机械隔振；不未经决定进入闭环 |
| PVR-008 | HAL49ESO 3.3 V 实际零点、灵敏度和磁场几何未知 | 已知极性/距离磁体 sweep，温度点复测 | Hall1 ADC/3V3；Hall2-4 DNP样板可选 | 不削顶、SNR满足 gameplay threshold | 饱和/漂移/极性错误 | 改 RC/gain/位置或经输入变更换有精确规格器件 |
| PVR-009 | WS2812C-V6 在 noisy 3.3 V rail 虽有原厂支持仍可能数据误码 | DIB-004 最终允许的 rail minimum、servo/audio/RF worst case，长时间 pattern test | DIN at LED1、每颗 VDD、DOUT chain | rail 不低于原厂 3.3 V 下限且无 spontaneous color/error/reset | rail 低于 3.3 V、flicker、错误色或 reset | level shifter、独立滤波/rail、加大 local decoupling |
| PVR-010 | ST7789 模块峰值和 BLK 内接未知，0.5 A 只是冻结预算 | exact module 到货后全白/高亮/startup，BL on/off | 3V3 protected branch、BL pin current | ≤0.5 A branch且 3V3 满足 spec，BL control兼容 | 过流/启动跌落/BL pin 回灌 | 外接模块独立受控电源、降低背光、更新接口输入 |
| PVR-011 | NC ESTOP 长线可能受 bounce/EMI，或重新闭合时误 re-arm | 按下/拔出/断线/bounce/注入噪声；所有 source state 组合 | loop input、hardware latch、source enables、SERVO_V+ | 最终 shutdown time内关断且不自动恢复 | 漏切、误恢复、噪声误触发不可接受 | 增强滤波/Schmitt/ESD、改变 latch/connector/harness |
| PVR-012 | physical fuse 在真实电池/温度/PCB散热下的 clearing 与 nuisance trip 不确定 | 不在当前阶段执行；使用受控 fault fixture、屏蔽、限能和合格人员，先做非破坏性负载曲线 | fuse current/voltage/temperature、下游 energy | 8 A波形不误断；规定 catastrophic fault 在最终时间内安全清除 | nuisance trip、弧光、PCB damage或不清除 | 改 fuse family/rating、加强 upstream harness fuse、重调 active protection |
| PVR-013 | high-current connector/harness contact resistance 主导压降和热点 | 最终配对件/压接/线径，6 A steady/8 A transient，振动前后复测 | 每对 contact millivolt drop/temperature | 满足 DIB-002/003 和 connector derating | 热点、压降超限、接触间歇 | 换 connector/线径、并联电源触点、应力释放重做 |
| PVR-014 | **RETIRED by v0.6**：原YSIDO fallback验证风险 | 不再执行；v0.6已删除外置UBEC接口和assembly option | N/A | N/A | 若实物/文档重新出现外置6 V source即违反v0.6 | 停止并通过新Design Input revision重新审计 |
| PVR-015 | `SY8388ARHC` paper design覆盖6 A/8 A，但实际效率、温升、舵机动态、regeneration、hiccup/auto-recovery和EMI必须样机验证 | 唯一板载buck装配；9.0/12.6 V输入、TA=40 °C/封闭机身等效预热；6 A至热稳态，8 A/1 s且事件起点≥10 s；组合启动、受控短路、regen、ESTOP/MASTER cycle；先电子负载后固定执行器 | `SERVO_BUCK_IN`、IN/LX/BUCK_6V/SERVO_V+、PG/EN/ILMT、IC/inductor/capacitor/connector温度、近场EMI | 6 A/8 A波形、压降、温升、TJ估算及默认/故障状态满足v0.6；无reset、振荡、不可控恢复、source fighting或超额负向电压 | rail越界、热超限、inductor saturation、hiccup误触发、EMI/音频干扰、regen过压或ESTOP超时 | 调整L/C/MODE/ILMT/filter/copper；仍失败则更换buck并重跑全部受影响计算；不得恢复已删除外置UBEC fallback |
| PVR-016 | OpenCat成熟多舵机实现曾记录共享timer、归零和detach/re-attach过程导致残余PWM噪声或舵机跳动；这不是当前CH32方案的直接失效证据，但提示reset/watchdog/关断时序必须覆盖 | 首先保持舵机电源断开，以逻辑分析仪/示波器同时观察8路：CH32 POR/NRST/SWIO烧录、host reset、I2C中断/无效帧、100 ms watchdog、atomic commit、TCA `/OE`、ESTOP及MASTER cycle；通过后才在固定、限位、限流条件下接单路舵机复测 | CH32 timer pins、74HC244 `/OE`及8路buffer输出，触发参考为NRST/I2C/ESTOP/servo-source enable | 禁用期间输出保持已定义low/Hi-Z；任一关断/复位转换均不得产生`500–2500 µs`舵机有效窗口内的孤立脉冲，且硬件`/OE`先于timer停止完成禁用；重启时全部channel先配置inactive再统一enable，并继续满足EDD-015的50 Hz、≤2 µs相位偏差、atomic update和watchdog要求 | 出现有效窗口残余脉冲、任一路在禁用后持续输出、enable与未初始化timer重叠、通道相位/atomic/watchdog不满足EDD-015，或固定舵机产生非指令运动 | 先修正CH32初始化/关断顺序、timer pin默认态、pull与`/OE`控制；必要时增加硬件blanking/Schmitt/RC并重测；只有无法满足安全窗口时才重新比较PCA9685，不得通过删除通道或功能规避 |
| PVR-017 | LM74800功能描述与25 °C器件漏电不能保证实际PCBA在高温、污染、跨域网络和所有关断状态下满足每端口`≤100 µA` | 按v0.7和`EDD002_LM74800_LEAKAGE_EOL_v0.1.md`：首批每板25±5/40±3 °C跑N/O/E/F/Z/H完整矩阵；两SMU四线、blank fixture、U95、60 s settle/10 s sample；设计鉴定增加70 °C裕量和MASTER/ESTOP/INA300各100次循环 | `VBAT_RAW`、`VBAT_PROTECTED/SERVO_BUCK_IN`、LM enable/status、每个受影响external power pin的signed `I_PORT`和整板`I_TOTAL` | 首批每板所有状态25 °C `abs(I_PORT)+U95≤50 µA`、40 °C`≤75 µA`且永不超过100 µA；数据按serial/fixture/instrument/BOM/lot可追溯 | 任一点超guardband/100 µA、达到25 mA compliance、不能稳定、状态或电压失控、数据不可追溯 | 隔离FAIL板；清洗/修复跨域路径/更换有高温maximum的FET或controller/改fixture；返修后重跑两温度全矩阵，不得仅复测失败项或固件绕过 |

## 7. Circuit Design 工作分解

| Work package | 输入 | 产出/关闭条件 | Depends on |
| --- | --- | --- | --- |
| WP-01 Input closure | 本文 DIB | 更新后的权威 Design Input Freeze；DIB 全 CLOSED | Operator |
| WP-02 Component evidence | exact MPN 清单 | 每个承担纸面保证的器件具有 manufacturer document metadata、pin/package/ratings audit；CDB 全 CLOSED | 当前 CDB 已清零；仍需完成 power MPN 与 connector/switch 选型 |
| WP-03 Power/fault architecture | WP-01/02 | complete power tree、fault matrix、MASTER/ESTOP truth tables、默认状态 | Design Input v0.5 available；等待工程选型/计算 |
| WP-04 Reproducible power calculations | 已选 power parts | fuse/eFuse/FET SOA、drop、thermal、UV/OV、inrush、mux、buck、capacitor、derating 附件和运行命令 | WP-03 |
| WP-05 Digital/analog/audio circuits | verified datasheets | 每器件 pin-by-pin connection、filter/decoupling/protection、I2C address/default states | WP-02/03 |
| WP-06 GPIO and expander assignment | Freeze 的 15 GPIO 规则 | complete ESP32/TCA/PCA assignment；脚数/strap/ADC/PSRAM 自动检查通过 | WP-05 |
| WP-07 Connector/test-point/DNP tables | exact connector drawings | every connector pin、test point、jumper、DNP/assembly table | WP-02/06 |
| WP-08 Design verification package | WP-03..07 | startup/shutdown/power-loss/fault states、6 A/8 A analytical verification、prototype matrix、traceability | all above |
| WP-09 Proposed Circuit Design Freeze | DIB/CDB=0 | 仅生成 `Proposed / Review Candidate`；运行完整性/计算/Markdown/仓库测试 | WP-08 |
| WP-10 Human review | Proposed candidate | Operator Review → Independent Human Review → comment resolution；AI 不改 Accepted 状态 | human authorization |

## 8. 技术资料登记（初始批次）

访问日期均为 `2026-08-30`。未写发布日期表示正文未给出，不能用搜索引擎时间代替。

| Manufacturer | Exact MPN / scope | Document title | Document no. / revision / date | Source |
| --- | --- | --- | --- | --- |
| Espressif Systems | `ESP32-S3-WROOM-1-N16R8` | ESP32-S3-WROOM-1 & ESP32-S3-WROOM-1U Datasheet | v1.8 / 2026-03-02 | [manufacturer PDF/HTML](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.html) |
| Espressif Systems | `ESP32-S3` chip GPIO/strap scope | ESP32-S3 Series Datasheet | v2.2 / 2026 | [manufacturer PDF](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf) |
| TDK InvenSense | `ICM-42670-L` | ICM-42670-L Datasheet | DS-000431 / Rev. 1.1 / 2021-03-18 | [authorized-distributor-hosted manufacturer PDF](https://datasheet.lcsc.com/datasheet/pdf/874d19cb3cf7cf4a72e58391bfc479df.pdf?productCode=C49073546) |
| Hallwee Electronics | `HAL49E SO` (`HAL49ESO`) | HAL49E 线性霍尔传感器 | v3.2 / date not stated | [authorized-distributor-hosted manufacturer PDF](https://datasheet.lcsc.com/datasheet/pdf/4c30f55e3c49f4ca4a892a3b844ea483.pdf?productCode=C7420984) |
| LinkMems Acoustic Technology | `LMD3526B261-OFA03` | Specification of MEMS Microphone | No. M196 / v1.0 / 2023-11-17 | [authorized-distributor-hosted manufacturer PDF](https://atta.szlcsc.com/upload/public/pdf/source/20240812/81C7E69BC215A76B37A1D0E560FFDF0D.pdf) |
| Worldsemi | `WS2812C-2020-V6` | 智能外控集成 LED 光源 | v1.0 / 2026-05-21 | [authorized-distributor-hosted manufacturer PDF](https://datasheet.lcsc.com/datasheet/pdf/8c7f104c99e52e4adb687ae84e594e95.pdf?productCode=C55109522) |
| Analog Devices/Maxim Integrated | `MAX98357AEWL+T / C2682619` | MAX98357A/MAX98357B: Tiny, Low-Cost, PCM Class D Amplifier with Class AB Performance | 19-6779 / Rev. 11 / 2018-05 | Operator-provided LCSC-hosted manufacturer copy；SHA-256 `506fc7f9...6674f189`；[manufacturer product family](https://www.analog.com/en/products/MAX98357A.html) |
| TECH PUBLIC | `TPS3808G33DBVR-TP / C51886189` | TPS3808 Series-TP Low Quiescent Programmable-Delay Supervisory | document no./revision/date not stated | Operator-provided LCSC-hosted manufacturer copy；SHA-256 `182b6e24...09e5d0a`；[LCSC item](https://www.lcsc.com/product-detail/C51886189.html) |
| Megastar | `ZX-MX1.25-2PZZ / C7430458` | MX1.25 2P / ZX-MX1.25-2PZZ manufacturer drawing | version 2.0 / document date not stated | Operator-provided LCSC-hosted manufacturer drawing；SHA-256 `11c9a5c1...40a47a7` |
| Changzhou Amass | `XT30UPB30-M.G.* / C431092` | XT30UPB30-M.G.* manufacturer specification/drawing | document no./revision/date not stated | [LCSC item C431092](https://www.lcsc.com/product-detail/C431092.html)；Operator-provided local manufacturer copy `E378FC9AC063FF51965EEF4A8808ED65.pdf`；SHA-256 `02372b47...ca5952`；PDF 不提交仓库 |
| Changzhou Amass | `XT30U-M.N.B / C19268019` | XT30U-F.N.B / XT30U-M.N.B manufacturer specification | D-1542 / 2021V1 | [LCSC item C19268019](https://www.lcsc.com/product-detail/C19268019.html)；Operator-provided local manufacturer copy `672F8D75D4D23EED3192CF96B37AE06B.pdf`；SHA-256 `50057652...070f4`；PDF 不提交仓库 |
| Hanxia | `HX KCD11-2P-R / C6917318` | HX KCD11-2P-R manufacturer drawing | document no./revision/date not stated | [LCSC item C6917318](https://www.lcsc.com/product-detail/C6917318.html)；Operator-provided local manufacturer copy `919A4BACF8680158ADA16BF460B35412.pdf`；SHA-256 `51b39af9...12c2b18a66`；PDF 不提交仓库；panel-mount，因此未选择 |
| Ningbo Kangnex Electric / KANGNEX | `WJ2EDGVC-5.08-02P-14-00A / C8445` board socket | WJ2EDGVC-5.0/5.08 Plug-in Terminal Block / exact JLC item data | manufacturer page version 1.0 download context；exact drawing revision/date not recorded | [manufacturer product page](https://www.china-wj.com/en/product_detail1/id/670.html)；[JLC C8445](https://jlcpcb.com/partdetail/Ningbo_KangnexElec-WJ2EDGVC_5_082P/C8445)；accessed 2026-08-30 |
| Ningbo Kangnex Electric / KANGNEX | `WJ2EDGK-5.08-02P-14-00A / C71370` wire plug | WJ2EDGK-5.08-XXP-1Y-00A Customer Drawing | Rev. A / 2024-03-10 | [manufacturer product/adapted-products page](https://www.china-wj.com/cms/en/prod/id/326.html)；[exact manufacturer drawing hosted by LCSC](https://datasheet.lcsc.com/lcsc/1912251642_Ningbo-Kangnex-Elec-WJ2EDGK-5-08-2P_C71370.pdf)；[JLC C71370](https://jlcpcb.com/partdetail/Ningbo_KangnexElec-WJ2EDGK_5_082P/C71370)；accessed 2026-08-30 |
| NXP Semiconductors | `PCA9685PW,118` — superseded by v0.4 | PCA9685 16-channel, 12-bit PWM Fm+ I2C-bus LED controller | Rev. 4 / 2015-04-16 | [manufacturer PDF](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf) |
| NXP Semiconductors | shared I²C design | I2C-bus specification and user manual | UM10204 / Rev. 7.0 / 2021-10-01 | [manufacturer PDF](https://www.nxp.com/docs/en/user-guide/UM10204.pdf) |
| Texas Instruments | `TCA9539PWR` | TCA9539 Low Voltage 16-Bit I2C and SMBus Low-Power I/O Expander | SCPS202C / Rev. C / revised 2016-05 | [manufacturer PDF](https://www.ti.com/lit/ds/symlink/tca9539.pdf) |
| Texas Instruments | `TUSB320LIRWBR` candidate | TUSB320LI, TUSB320HI USB Type-C Configuration Channel Logic and Port Control | Rev. D / 2017-05-02 | [manufacturer product page](https://www.ti.com/product/TUSB320LI) |
| Analog Devices | `LTC4368IMS-2#PBF` selected controller | 100V UV/OV and Reverse Protection Controller with Bidirectional Circuit Breaker | Rev. C / publication date not stated | [manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc4368.pdf) |
| Texas Instruments | `LM74800-Q1` candidate | LM7480-Q1 Ideal Diode Controller with Load Dump Protection | Rev. C / date not recorded in current audit | [manufacturer product page](https://www.ti.com/product/LM7480-Q1) |
| Texas Instruments | `TPS25982` family candidate | TPS25982 2.7-V to 24-V, 2.7-mΩ, 15-A Smart eFuse | Rev. D / date not recorded in current audit | [manufacturer product page](https://www.ti.com/product/TPS25982) |
| Bel Fuse | `0685H9100-01` candidate | C1H Series — High Current Rated Fast Acting Chip Fuse | current manufacturer datasheet; publication date not stated | [manufacturer datasheet](https://www.belfuse.com/media/datasheets/products/circuit-protection/ds-cp-c1h-series.pdf) |
| WCH | `CH32V003F4P6 / C5187096` | CH32V003 数据手册 | V1.8 / PDF metadata 2025-10-17 | Operator-provided local manufacturer copy `C5187096_单片机(MCU-MPU-SOC)_CH32V003F4P6_规格书_WJ1791102.PDF`；SHA-256 `2c05b25a3ef9269ca3685db09de05a2b31c650e1b389b0cecc735d4176985b91` |
| Nexperia | `74HC244PW,118 / C5623` | 74HC244; 74HCT244 Octal buffer/line driver; 3-state | Rev. 7 / 2021-07-27 | Operator-provided local manufacturer copy `C5623_缓冲器-驱动器-收发器_74HC244PW,118_规格书_WJ412673.PDF`；SHA-256 `7d5786046ad334ebf2072ef866e180b2464a12c3235ec5d6321e169e806044e7` |

## 9. 本轮状态摘要

- Open `DESIGN_INPUT_BLOCKER`：1；DIB-001..012已关闭，DIB-013要求定义电池入口TVS系统瞬态包络。
- Open `COMPONENT_DATA_BLOCKER`：0；CDB-001..004由器件资料关闭，CDB-005由v0.7接受的不减功能整板逐板EOL/qualification规则关闭，实测风险转入PVR-017。
- `ENGINEERING_DESIGN_DECISION`：24 项；EDD-003 retired。EDD-005已选择AON async ESTOP latch topology并完成RC timing；EDD-015已完成CH32/74HC244 pin mapping、I2C protocol、atomic update/watchdog/default-state contract；EDD-024已选择SY8388的2.7 µH、CIN/COUT/FB/MODE/ILMT电气值。EDD-004/006/007/008/009/010/012/023已收敛拓扑但等待exact power/protection MPN；EDD-001/002/013/022仍partial。
- Active `PROTOTYPE_VALIDATION_RISK`：16 项；PVR-014已由v0.6 retired，PVR-015为板载buck样机风险，PVR-016为PWM关断/复位波形风险，PVR-017为首批逐板反灌EOL/qualification风险。active PVR可保留到Proposed candidate，但必须带test/pass/fail/fallback；PVR-017在首批放行前必须PASS。
- Circuit Design Freeze candidate：**当前不允许生成**；DIB-013仍须关闭，且全部Engineering Design Decisions、exact power MPN、worst-case calculations及完整pin/net/default/fault/traceability检查仍须完成。CDB-005关闭不自动接受EDD-002。首次候选仍只能是 Proposed / Review Candidate。
