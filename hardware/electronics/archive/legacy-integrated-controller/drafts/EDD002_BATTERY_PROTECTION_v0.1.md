# EDD-002 电池入口有源保护筛选 v0.1（DRAFT 副本）

状态：**DRAFT / 非权威 / 未经 review**。不改动任何源文件，不宣告选择已冻结，不授权 schematic/layout/采购/测试。

日期：2026-08-30

Owner：Project maintainer（待审阅）

关联：
- [`../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md`](../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md) EDD-002
- [`EMPIRICAL_BOUND_SENSITIVITY_v0.1.md`](EMPIRICAL_BOUND_SENSITIVITY_v0.1.md)（电池短路电流与 breaking 边界）

可复现脚本：[`verify_edd002_battery_protection.py`](verify_edd002_battery_protection.py)

## 1. 范围与本副本能做什么

EDD-002 已选控制器 `LTC4368IMS-2#PBF`（latch-off，`RETRY=GND`），back-to-back NFET 候选 `HD303N080SG / C52766420`，`MASTER_ENABLE` 控 `SHDN`。UV/OV 窗口已由 `verify_battery_uvov_window.py` 关闭（UV falling 8.608–8.960 V、OV rising 12.728–13.234 V）。

本副本补齐剩余的：**MOSFET 导通损耗与热**、**RSENSE 选值**、**SOA 应力点**、**反向电流阈值**。使用 HD303N080SG 原厂 REV1.0 真实数据（30 V / RDS(on)≤8 mΩ@10V / RθJC=6 ℃/W / ID=28 A / IDM=100 A / PDFN3030）。LTC4368 的电流限制阈值、TMR、CGATE 常数为 ASSUMPTION，需以 Rev C datasheet 核实（见 §5）。

## 2. 发现

### 2.1 导通损耗与热（真实 RDS，闭合）

back-to-back 两只 FET 串联，RDS 取 max 8 mΩ @10V、热乘 1.5（Fig.2 典型，ASSUMPTION）：

| 工况 | P_total | 单管 P | TJ（RθJA=40–60 ℃/W 假设） | 结论 |
| --- | --- | --- | --- | --- |
| 6 A continuous | 0.864 W | 0.432 W | 57–66 ℃ | OK（≪100 ℃ 稳态界） |
| 8 A 总瞬态 | 1.944 W | 0.972 W | 79–98 ℃ | OK（1 s/10% 占空比） |

**结论**：导通/热不是限制因素，HD303 的 8 mΩ 在这套电流下余量充足。

### 2.2 RSENSE 选值（建议 4 mΩ）

总电流包络 = buck 支路（6 A cont / 7.5 A·1 s / 10 A·10 ms）+ LOGIC_BAT（ASSUMPTION 1.5 A 上界）→ 连续 7.5 A、瞬态 9.0 A、浪涌 11.5 A。

| RSENSE | I_trip min/typ/max（40/50/60 mV 带） | 对 9 A 包络余量 | 判定 |
| --- | --- | --- | --- |
| 4 mΩ | 10.0 / 12.5 / 15.0 A | +1.0 A | ✅ 建议 |
| 5 mΩ | 8.0 / 10.0 / 12.0 A | −1.0 A | ❌ 拒绝 |
| 6.2 mΩ | 6.5 / 8.1 / 9.7 A | −2.6 A | ❌ 拒绝 |

**结论**：`RSENSE = 4 mΩ`（±1%，Kelvin，2512/1 W 级）。6 A 时 0.144 W、8 A 时 0.324 W，功耗可接受。

**重要 caveat**：+1.0 A 余量基于本文假设的 VCL 下限 40 mV；LTC4368 Rev C 的 **guaranteed VCL min/max 必须先核实**——若真实下限更高（例如 45 mV），余量自动加大；若真实下限更低，则需要更小的 RSENSE（如 3.6 mΩ）。5 mΩ 与 6.2 mΩ 因 min trip 低于 9 A 包络而被拒绝。

### 2.3 浪涌与电流限制的交互（设计点）

启动浪涌 11.5 A 超过 4 mΩ 的 trip 下限（10 A）→ LTC4368 的电流限制**天然充当软启动**，把 buck 输入电容充电电流钳在 ~12.5 A。两种闭合方式，二选一：

1. 让电流限制接管浪涌，**TMR 必须长于浪涌时长（≥10 ms）**，避免误 latch-off；或
2. 用 **CGATE** 降低栅极爬升率，使 buck 输入电容充电电流 ≤ trip 值（需 LTC4368 栅极拉电流 + buck 输入电容 Cin 两个数据）。

### 2.4 SOA 应力点（待 Fig.8 核实）

下游硬短路时，若 LTC4368 做线性电流限制：`VDS≈12.6 V、ID≈12.5 A、P≈158 W`，持续时间为 TMR。该点必须对照 **HD303 Fig.8 SOA（VDS=12.6 V 列）** 确认 12.5 A 在所选 TMR 脉宽内允许；同时 TMR 应尽量短，压缩线性模式持续时间。

### 2.5 反向电流阈值（-2 变体）

反向阈值 −1…−5 mV（-2 变体，来自 EDD-002 记录）：
- 4 mΩ → 反向 trip 0.25–1.25 A；5 mΩ → 0.20–1.00 A。

用于防止 VBAT 反灌；该低阈值正是选 -2（而非 -1 的 −42…−58 mV）的理由，闭合。

## 3. 仍待闭合（需 LTC4368 Rev C 数据）

1. **VCL guaranteed min/max**：决定 4 mΩ 是否最终可用，或需降到 3.6 mΩ；
2. **TMR 方程与电容选值**：确认浪涌/短路时 latch-off 时序；
3. **CGATE 与栅极拉电流**：与 buck 输入电容 Cin 一起做浪涌设计（与 EDD-024 的输入电容衔接）；
4. **SOA Fig.8 逐点核对**：12.6 V / 12.5 A / tTMR 是否在界内；
5. **TVS 钳位 vs 30 V MOSFET**：HD303 是 30 V 器件，需 TVS 把快速瞬态钳在 <30 V（留 AV 余量）；LTC4368 OV 在 12.7–13.2 V 关断是慢保护，TVS 是快保护。

## 4. 残余风险 / PVR 映射

| 项 | 状态 |
| --- | --- |
| 导通/热 | 纸面闭合（真实 RDS） |
| RSENSE | 建议 4 mΩ，待 VCL spec 确认 |
| 反向电流 | 纸面闭合（-2 变体低阈值） |
| SOA 线性模式 | 待 Fig.8 + TMR，归入 PVR-012（fuse/protection clearing）的姊妹项 |
| 30 V TVS 钳位 | 待选件，AV 余量归 PVR |
| 浪涌/CGATE | 待 LTC4368 数据，与 EDD-024 输入电容衔接 |

## 5. 可复现命令

```sh
python3 hardware/electronics/drafts/verify_edd002_battery_protection.py
```

## 6. 边界声明

VCL 40/60 mV 带、热乘数 1.5、RθJA 40–60 ℃/W、LOGIC_BAT 1.5 A 均为 ASSUMPTION，非原厂保证、非实测。本文不宣告 EDD-002 闭合，不授权 schematic/layout/采购/测试；合入权威记录前须经 Operator 审阅。

## 7. 变更记录

| 日期 | 变更 | 由 |
| --- | --- | --- |
| 2026-08-30 | 初稿：EDD-002 导通/热、RSENSE、SOA 应力点、反向阈值 | Agent（待 Operator 审阅） |
