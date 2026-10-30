# 牛来快跑 文档地图

这里收录协议、架构、规格、硬件和工程记录。文档不是营销页：它们区分**已观测事实**、**假设/候选值**和**待验证项**。看到 “Testing”“Proposed”“HIGH RISK” 时，请不要把代码存在当成硬件验收。

> 第一次来？先读根目录 [`README.md`](../README.md)，再按下面的“入门路径”阅读。

## 入门路径

1. [项目 README](../README.md) — 项目是什么、怎么跑、安全边界。
2. [BLE GATT 协议 v1](BLE_PROTOCOL.md) — Android 与 ESP32 之间的唯一无线控制边界。
3. [ESP32 软件架构 v1](architecture/ESP32_SOFTWARE_ARCHITECTURE_v1.md) — HAL、驱动、服务、运动策略和安全门的分层。
4. [控制规格 v0](specs/CONTROL_SPEC_v0.md) — 单腿 Hold-to-Swing 的状态语义。
5. [玩法设计 v0.1](specs/GAMEPLAY_DESIGN_v0.1.md) — 单人/双人、有效步评分和赛后反馈。
6. [硬件总览](../hardware/README.md) — Carrier、电源、接口和制造资料。

## 协议与架构

| 文档 | 内容 |
| --- | --- |
| [`BLE_PROTOCOL.md`](BLE_PROTOCOL.md) | GATT 服务/特征、四字节控制包、STATUS、HOME、IMU、GAME_MODE、AUDIO_COMMAND、心跳和 500 ms failsafe。 |
| [`architecture/ESP32_SOFTWARE_ARCHITECTURE_v1.md`](architecture/ESP32_SOFTWARE_ARCHITECTURE_v1.md) | 选择性 HAL、设备驱动、平台服务、ARD-1 运动策略、安全监督器和依赖规则。 |
| [`architecture/INTERFACES_v0.md`](architecture/INTERFACES_v0.md) | `LegActuator`、`FootContactSensor`、`PlayerInput` 等最小能力边界。 |
| [`specs/CONTROL_SPEC_v0.md`](specs/CONTROL_SPEC_v0.md) | `SUPPORT → LIFT → SWING → SEARCH → SUPPORT` 与 `FAULT`。 |
| [`specs/GAMEPLAY_DESIGN_v0.1.md`](specs/GAMEPLAY_DESIGN_v0.1.md) | 游戏流程、双评分候选、音频反馈和软件边界。 |

## 决策记录

- [`decisions/INDEX.md`](decisions/INDEX.md) — 全部 Decision Record 的状态索引。
- [`decisions/OPEN_DECISIONS.md`](decisions/OPEN_DECISIONS.md) — 仍未冻结的选择。
- [`decisions/README.md`](decisions/README.md) — 如何写 Decision Record。
- [`decisions/TEMPLATE.md`](decisions/TEMPLATE.md) — 新记录模板。

Decision Record 记录“为什么这样选”，不替代实验数据。已经被取代的决定保留原文，并用 `Superseded` 链接到后继记录。

## 实验与证据

- [`../experiments/README.md`](../experiments/README.md) — 42 份编号实验的索引。
- [`../experiments/TEMPLATE.md`](../experiments/TEMPLATE.md) — 实验记录模板。
- [`../experiments/logs/`](../experiments/logs/) — 原始串口/台架日志。

实验记录应包含：硬件版本、接线、参数、软件 commit 或“未提交”、重复次数、原始日志位置、失败结果和下一步。**失败结果同样有价值，不要删除。**

## 排障与台架

| 文档 | 用途 |
| --- | --- |
| [`I2C_SERVO_TROUBLESHOOTING_GUIDE.md`](I2C_SERVO_TROUBLESHOOTING_GUIDE.md) | 从电源、线束、上拉到 PCA9685 的分层排障。 |
| [`NACK_POWER_DIAGNOSIS_CASE_STUDY.md`](NACK_POWER_DIAGNOSIS_CASE_STUDY.md) | 多舵机动作时 I²C NACK 的供电/公共地复盘。 |
| [`SERVO_ZERO_TCP_DIAGNOSTIC.md`](SERVO_ZERO_TCP_DIAGNOSTIC.md) | 隔离舵机固件的 SoftAP/TCP 诊断边界。 |
| [`AUDIO_LISTEN_TEST_RUNBOOK.md`](AUDIO_LISTEN_TEST_RUNBOOK.md) | MAX98357A + 扬声器的分档听测流程。 |

## 硬件资料

- [`../hardware/README.md`](../hardware/README.md) — 硬件总览。
- [`../hardware/electronics/MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](../hardware/electronics/MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md) — Carrier v0.2 电源、接口和机械约束。
- [`../hardware/electronics/HARDWARE_PARAMETER_REGISTER_v0.1.md`](../hardware/electronics/HARDWARE_PARAMETER_REGISTER_v0.1.md) — 未实测/未冻结参数登记表。
- [`../hardware/electronics/schematic/`](../hardware/electronics/schematic/) — KiCad 原理图、PDF/SVG、BOM、网络表、ERC。
- [`../hardware/electronics/manufacturing/gerber/`](../hardware/electronics/manufacturing/gerber/) — 制造 Gerber 与放行记录。

## 文档约定

- **事实 / 观测**：有日志、测量、截图或可复现命令支撑。
- **假设 / 候选**：为了推进实验而暂时采用，必须写明清除条件。
- **提议**：尚未被团队接受，不能写成“已决定”。
- **HIGH RISK**：涉及运动范围、力矩、速度、保护阈值、电源或急停；必须先 mock/host 验证，再由具备安全条件的台架执行。

修改行为时，请同步更新对应的协议、Spec、Decision 或 Experiment。README 只写当前可复现的状态，不提前宣称未验证能力。

## 机器检查

```bash
python3 tools/check_markdown_links.py
python3 tools/check_experiment_index.py
python3 tools/check_decision_index.py
```

完整无硬件入口：

```bash
sh tools/run_offline_checks.sh
```

这些检查只验证文档链接、索引和代码回归，不能替代实体 BLE、显示、音频、IMU、电源和舵机验收。
