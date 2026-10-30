# 牛来快跑 PCB Controller 设计输入冻结 v0.8

> **SUPERSEDED 2026-08-31**：当前权威输入已改为 [`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](../../MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md)。本文件只保留高度集成控制板方案历史，不得用于当前 Carrier schematic/layout。

状态：**Accepted Design Input / Operator 明确修订；尚未开始原理图或网表设计**

日期：2026-08-31

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md)

## 1. 修订范围与不变功能

本文完整取代 v0.7，并规范性纳入其未修改条款。本次修订来自 Operator 的明确决定：舵机 6 V 改由一块已有的外置 UBEC 提供；控制板不再接收原始 3S 电池、不再板载 6 V servo buck、USB POWER 或任何舵机高电流 source-selection/power-management。

以下功能不删除：`ESP32-S3-WROOM-1-N16R8`、全部板载传感器/音频/灯/扩展、CH32 八路硬件 PWM、USB DATA 调试、PWM 默认安全态、MASTER/NC ESTOP 的系统语义。删除的是板内高电流实现，不是 ESTOP 或安全关断功能本身。

外置 UBEC 的 6 A continuous / 8 A short 能力、输出质量、反接、短路、回生和热行为由 Operator 明确作为本轮系统假设；本板不把商城宣称伪装为元件资料保证。实际舵机系统仍须验证其 6 A/8 A 包络。

## 2. 新的单电源接口架构

控制板只有一个正常电源入口：`EXT_6V_IN`，使用 Operator 指定的 DC5.5 barrel interface（中心正极；内径究竟为 2.1 mm 还是 2.5 mm 必须在选 exact jack 前确认）。此接口只给控制板低电流逻辑支路供电，**不是舵机电源分配接口**。最终 exact jack MPN 必须证明连续额定电流覆盖控制板逻辑输入，并在 symbol/footprint 明确中心正极。

```text
3S LiPo ──> external UBEC, set to 6.0 V ──> EXTERNAL_6V_BUS ──┬─> external high-current ESTOP ─> servo PDB ─> eight servos
                                                               │
                                                               └─> matching DC5.5 plug ─> EXT_6V_IN (controller board)
                                                                                   │
                                                                                   └─> 6V_LOGIC ─> 3V3 buck ─> logic

bench 6.0 V current-limited PSU ─────────────────────────────> EXTERNAL_6V_BUS
                                        (UBEC physically unplugged during this mode)
```

UBEC 与台式电源是同一 `EXTERNAL_6V_BUS` 的**互斥替代 source**，不得并联或热插拔切换。测试单独控制板时，可仅把台式电源接到 `EXT_6V_IN`；接舵机测试时，台式电源应接上述外部总线并满足所测负载的电流能力，不能借控制板 DC5.5 端口承载舵机电流。

控制板的 `EXT_6V_IN` 前端至少包含低电流可恢复/可更换保护、反接保护和明确的 6 V input/ground test point；exact MPN 和压降/热计算仍属于 Circuit Design。`6V_LOGIC` 经低成本 buck 产生 3V3；`SY8113BADC` 是成本候选，若采用无 PG 型号，独立 reset supervisor 必须维持既有 brownout/default-safe 语义。

## 3. USB 与共地规则

USB-C 保留为 **DATA-only**：D+/D−、CC 和 GND 按 v0.7 的 USB DATA 连接规则保留。电脑 VBUS 不进入任何板内供电树；若保留 VBUS-presence detect，只能经高阻、掉电安全的隔离检测网络。故 USB 线可以在 UBEC/台式 6 V 供电时同时连接，不构成第二个供电 source，也不得向电脑 VBUS 反灌。

控制板 GND、UBEC 输出 GND、servo PDB GND 与 USB GND 共享同一低阻参考；八路 PWM 仅从 CH32→74HC244 输出，保留每路 100 kΩ default-low 和 `/OE` 默认禁用链。禁止让 PWM 线承担任何供电或回流职责。

## 4. 外置安全边界

外置 high-current ESTOP 必须位于 `EXTERNAL_6V_BUS → servo PDB` 支路，能断开八路舵机 V+，且其 NC loop、latch/clear 和 MASTER OFF→ON 恢复行为必须保持原系统语义。控制板逻辑 6 V 支路不随该 ESTOP 断开，以允许故障后 USB 调试；控制板仍通过 74HC244 `/OE` 保证 PWM 默认无有效脉冲。

若未来把 DC5.5 barrel 用作舵机 6 V 输入、让舵机电流经过控制板，或重新加入 raw 3S/USB POWER source，均属于新的架构变更，必须重新完成高电流、反灌、ESTOP、保护及热设计；不得以本 v0.8 作为依据。

## 5. 从 v0.7 删除的板内功能件

以下项不得进入 v0.8 控制板 net/component/assembly table：

- raw 3S battery connector、physical fuse、LM74800、INA300、battery MOSFET pair、battery shunt、AON battery LDO 与 battery-entry TVS；
- `SY8388ARHC`、其 2.7 µH 电感、输入/输出 bulk、buck-to-servo isolation、servo current sense、servo regeneration clamp；
- USB POWER Type-C controller、USB eFuse、TPS2121 source mux、USB servo-source isolation；
- 为上述电源树服务的 EOL leakage matrix。USB DATA VBUS 的孤立 presence detect 不在此删除项内。

原 v0.7 的 LM74800 EOL/PVR-017 是被删除 raw-battery front end 的历史验证记录，不适用于 v0.8；新设计仍需对 `EXT_6V_IN`、USB DATA 和 PWM safe-default 完成对应的原理图/台架验证。

## 6. 成本基线（已定价功能件）

按 Operator 2026-08-31 价格表，保留功能件为 ¥45.619/板。以 `SY8113BADC` 约 ¥1.420 作为 6 V→3.3 V buck 的成本候选，控制板已定价功能件为：

```text
¥45.619 + ¥1.420 = ¥47.039 / board
```

UBEC 已由 Operator 持有，故不计入新控制板采购成本。此数不含 DC5.5 jack、低压保护、buck 电感/电容、去耦/阻容、外置 servo PDB/ESTOP/harness、PCB 或贴装，不能称作完整 PCBA 价格。

## 7. 阶段边界

本修订授权更新 Circuit Design 文档和成本模型；仍不授权 schematic、ERC、netlist、PCB layout、采购、下单或实体测试。下一步必须先把 v0.8 的 `EXT_6V_IN` 低压保护、DC5.5 exact MPN、external ESTOP/PDB interface、3V3 buck/supervisor 与 USB DATA VBUS isolation 收敛为可审阅连接表。

## 8. 制造与报价约束（Operator 2026-08-31）

- PCB：4层、1.0 mm、单面 SMT、Economic compatible、首批 5 块。
- 机贴扩展库最多 3 种；Standard-only 器件数量为 0。
- `ESP32-S3-WROOM-1-N16R8` 及易焊扩展 IC（优先 TSSOP/SO/QFN 等可人工检查封装）安排后焊，不计入机贴扩展库名额；后焊不能使用飞线，必须有正式焊盘、丝印和检验点。
- WLP、底部声孔、天线和 USB 等高风险器件只有在供应商确认 Economic 工艺可稳定贴装时才允许机贴；否则改为有正式 footprint 的后焊/替代封装，不得牺牲功能。
- 5块价格必须单列 PCB 阶梯费、SMT 费、扩展库费和后焊人工费；不能用“Standard-only=0”隐藏后焊成本。
- 最终板必须无飞线、无外挂功能模块；外置 UBEC、舵机 PDB/ESTOP 属系统线束，不是板上功能模块，必须在接口和装配说明中明确定义。
