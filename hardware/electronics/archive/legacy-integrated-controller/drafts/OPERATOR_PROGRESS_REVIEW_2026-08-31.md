# PCB Controller Rev A — Operator progress review (2026-08-31)

状态：**Review notes / 非 Circuit Design Freeze**。本文审核 Operator 在 `hardware/electronics/drafts/` 新增的 EDD-001、EDD-002 和经验边界材料；不改变权威 Design Input Freeze，不授权 schematic、ERC、netlist、PCB 或实体测试。

## 1. 已确认的推进

| 项目 | 审核结论 |
| --- | --- |
| 工作分支 | 当前仍为 `hw/pcb-controller-reva-circuit-design`，未发现切换到 `main`。 |
| EDD-001 方向 | 可插拔、陶瓷填砂 5×20 mm + PCB holder 是合理的工程方向；相比原 1206 保险丝改善可维护性和预期分断能力。**但 exact fuse/holder MPN 尚未确定。** |
| EDD-002 控制器 | `LTC4368IMS-2#PBF` + 背靠背 `HD303N080SG / C52766420` 仍可作为候选拓扑。UV/OV 脚本已有独立历史计算；新稿补充了 FET 导通损耗、RSENSE 和 SOA 应力点。 |
| 经验边界 | 明确把电池内阻、短路电流、舵机堵转、SY8388 热损耗标成 ASSUMPTION/PVR，未伪装成实测，这是正确的证据分层。 |
| 可复现性 | `python3 hardware/electronics/drafts/verify_edd002_battery_protection.py` 和 `python3 hardware/electronics/drafts/verify_empirical_bounds.py` 均可运行并输出结果。 |

## 2. 必须修正或保持开放的事项

### 2.1 EDD-001：选择方向已收敛，器件选择未闭合

新稿同时存在以下两层信息：

1. Operator 方向决定：5×20 mm 陶瓷填砂、可插拔 holder、目标 breaking ≥250 A；
2. 表格和经验脚本仍引用 `C355692 / 1206 / 150 A DC`。

因此当前应标记为 **TOPOLOGY SELECTED / EXACT MPN PENDING**，不能写成 `C355692` 已选。必须由 Operator 查询并提供：

- 保险丝 exact MPN、额定电压、额定电流、fast-acting time-current curve；
- 在不低于 32 VDC 下的 DC breaking rating（目标 ≥250 A）；
- holder exact MPN、接触电阻/额定电流、焊盘图和保持力资料；
- 保险丝与 holder 的允许温升及 8 A/1 s、≥10 s 间隔下的 no-nuisance 证据。

在此之前，`C355692` 只能保留为历史 SMD 参考，不能进入未来冻结网表。

### 2.2 EDD-002：电流包络文字与脚本需统一

新稿的文字使用：

- 冻结的板载 buck 输入包络：6.0 A continuous、7.5 A/1 s、10 A/10 ms；
- 另加 `LOGIC_BAT = 1.5 A` 的 ASSUMPTION，得到 7.5 A continuous、9.0 A transient、11.5 A inrush。

脚本的 `8 A total transient` 标签实际使用的是 `7.5 + 1.5 = 9.0 A`，不是冻结的舵机 8 A。这个做法可以作为保守“电池总输入包络”，但必须改名为 `7.5 A servo transient + 1.5 A logic bound = 9.0 A battery transient`，否则会造成验收误读。

另外，`LOGIC_BAT = 1.5 A` 尚未来自冻结要求或具体 regulator/load budget，故仍是 ASSUMPTION；在确定 3V3 source/mux 后须重跑。

### 2.3 LTC4368 参数仍是闭合门槛

新稿明确列出但尚未提供原厂保证值的项目，仍必须保留为开放工程决策：

- VCL guaranteed min/typ/max；
- TMR 方程、计时电容范围和 latch-off 时序；
- CGATE 充电电流、buck 输入电容与启动浪涌的联合计算；
- LTC4368-2 的反向断路器在实际 FET/RSENSE/布局下的行为；
- `HD303N080SG` Fig.8 SOA 在约 12.6 V、限流电流、TMR 脉宽下的逐点判定。

在这些数据完成前，`RSENSE=4 mΩ` 只能是候选值。当前脚本的 40/50/60 mV 是敏感性假设，不是 LTC4368 guaranteed specification。

### 2.4 30 V MOSFET 与 TVS 约束

`HD303N080SG` 的 VDS=30 V，而电池输入最高 12.6 V。TVS 需在所有允许故障/线束寄生条件下把 MOSFET 漏源电压限制在其额定值内，并留出动态余量；仅写“选择 TVS”不足以闭合。必须提供 exact TVS MPN、VRWM、VC@指定脉冲、功率/重复能力，并将它与 LTC4368 OV 慢关断、保险丝和输入电容共同计算。

### 2.5 热计算的证据边界

`RθJA=40–60 °C/W`、RDS 热乘数 1.5、开关上升/下降时间 10–30 ns 和 SY8388 10% 瞬态占空比均被新稿标为 ASSUMPTION，不能作为保证。它们可以继续作为敏感性筛选，但 Circuit Design Freeze 必须同时列出：原厂热阻适用的铜面积/层数、实际板叠层假设、损耗模型和 PVR 测试条件。

## 3. 当前状态判定

截至本审查：

- `DESIGN_INPUT_BLOCKER`：0（v0.6 仍是唯一权威输入）；
- `COMPONENT_DATA_BLOCKER`：0（但 exact fuse/holder/TVS/RSENSE/3V3/USB source MPN 尚未完成工程选型）；
- EDD-001：**PARTIAL — topology selected, exact MPN pending**；
- EDD-002：**PARTIAL — controller/FET candidate and arithmetic screen, guaranteed limits/SOA/TMR/TVS pending**；
- EDD-024：**PARTIAL — SY8388 architecture/MPN selected, full buck passive and thermal calculation pending**；
- Circuit Design Freeze：**不可提出**，因为仍有多个 EDD 未完成，且完整 pin/net/default/fault/connector 表尚未审查。

## 4. 下一步最小工作包

1. Operator 提供 5×20 fuse 与 holder 的 exact JLC MPN/原厂资料；保留 ≥250 A 作为工程目标，不自动升级为 Design Input 新硬要求。
2. 完成 LTC4368 Rev C 参数核对，并统一“舵机包络”和“电池总输入包络”的命名。
3. 选择并提供 TVS、4 mΩ Kelvin shunt、SY8388 电感和输入/输出电容候选；然后重跑 buck/保护联合计算。
4. 再收敛 USB 5 V source、3V3 regulator、servo OR/isolation 和 common current-sense topology。
5. 只有上述结果写入权威 open-issues 后，才生成详细的 `PCB_CONTROLLER_CIRCUIT_DESIGN_REVIEW_DRAFT` 供人类验收。
