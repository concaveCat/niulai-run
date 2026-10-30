# EDD-002 候选复核：LM74800QDRRRQ1 + INA300AIDGSR v0.1

状态：**Review / candidate only**。本文审核外部方案，不改变 `PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md`，不宣告 EDD-002 闭合，不授权 schematic、ERC、netlist、PCB 或实体测试。

## 1. 初步结论

`LM74800QDRRRQ1 / C3215600 + INA300AIDGSR / C529558 + 2×HD303N080SG / C52766420` 是当前比 `LTC4368IMS-2`、`LMX5069 + MX74610` 更有希望的低成本候选。

可接受的候选拓扑为：

```text
VBAT_RAW
  → 10 A / 5×20 ceramic fuse + holder
  → SMBJ16A TVS
  → 5 mΩ Kelvin shunt
  → LM74800-controlled back-to-back HD303N080SG pair
  → SERVO_BUCK_IN
```

`INA300` 只负责快速过流检测；其 ALERT 通过小信号 wired-kill 关闭 LM74800 EN/UVLO，避免让主 MOSFET 在毫秒级线性限流区工作。这一架构层面的简化是实质性优点。

## 2. 已复核的纸面项目

### 2.1 UV 分压

采用 `RUV_TOP=59.0 kΩ ±1%`、`RUV_BOTTOM=10.0 kΩ ±1%`，按给出的 LM74800 EN/UVLO threshold 1.091–1.267 V 计算：

- UV falling worst-high ≈ `8.14 V`；
- UV rising worst-high ≈ `8.89 V`；
- 因此 falling trip 确实低于冻结的 9.0 V 下限。

这满足冻结要求，但实际 EN/UVLO pin leakage、分压布局和 MASTER/ALERT 拉低网络仍须并入完整 corner 表。

### 2.2 OV 分压

采用 `ROV_TOP=100 kΩ ±1%`、`ROV_BOTTOM=10.0 kΩ ±1%`，按 rising threshold 1.195–1.267 V 计算：

- OV rising worst-low ≈ `12.91 V`；
- OV rising worst-high ≈ `14.20 V`；
- nominal 值应明确使用 datasheet 的 typical threshold，不能把 13.54 V 与 guaranteed corner 混写。

这满足 12.6 V 满电不误关和 rising >12.6 V 的冻结要求。

### 2.3 INA300 过流候选

`RSENSE=5 mΩ ±1%`、`RLIMIT=2.74 kΩ ±1%` 的 nominal threshold 约 `54.8–55 mV`，对应约 `11 A`。冻结包络中的 6 A、7.5 A、9 A 和 10 A/10 ms 分别对应 30、37.5、45 和 50 mV，均低于 nominal threshold。

但是，最终 trip 必须使用：

```text
Itrip,min = (Vlimit,min − |VOS,max|) / Rshunt,max
Itrip,max = (Vlimit,max + |VOS,max|) / Rshunt,min
```

其中 `Vlimit` 必须包含 ILIMIT 电流源、RLIMIT 和温度的保证误差；`VOS` 必须使用 datasheet 全温最大值。只有在 `Itrip,min > 9 A` 且 `Itrip,max ≤ 12 A` 时，才能写成满足冻结的 10–12 A 过流范围。

## 3. 尚不能写成 PASS 的项目

### 3.1 反向电流 ≤100 µA

LM74800 的 closed-loop reverse-current regulation 是有利条件，但不能仅以“zero DC reverse current”作为 `≤100 µA` 的保证证明。需要区分：

- `SERVO_BUCK_IN` 被外部抬高、`VBAT_RAW` 断开时的 VOUT→VIN 反灌；
- A/C pin leakage；
- 两颗 HD303N080SG 的 `IDSS`；
- TVS、分压、INA300 shunt 和 always-on LDO 的旁路路径；
- MASTER off、LM74800 disabled 和电池反接时的 leakage。

尤其是资料中“输入反接时 A pin 的 IREV”不能直接等同于正向电池已断开时的 VOUT→VIN 反灌。必须取得 exact datasheet 的测试条件和 maximum 值，再做系统级 leakage budget；最后仍建议样机实测。

### 3.2 TLV70433 的 always-on 供电

INA300 必须在 LM74800 latch-off 后继续供电，因此其 3.3 V 必须取自保险丝之后、主 FET 之前。但 `TLV70433` 的输入反接耐受、反向电流和输出电容放电路径不能假定存在。

还存在一个额定值冲突：`SMBJ16A` 的给定最大钳位约为 26 V，而 `TLV70433` 的输入额定上限为 24 V。若 TLV704 输入与 TVS 后的 AON 电池节点直接相连，TVS 钳位事件可能超出 LDO 输入额定值。因此当前 `SMBJ16A + TLV70433` 组合不能标记为 PASS。

必须补充以下任一措施：

- 在 TLV704 输入前增加明确额定的串联反接阻断器件；或
- 在 TLV704 输入前增加限流/箝位，使 LDO 端 worst-case 电压低于其额定值；或
- 改用输入额定值高于 TVS 最大钳位的 AON regulator；或
- 选择原厂资料明确支持反接/反向电流场景的 AON regulator；或
- 证明 LM74800 的 AON/受保护节点能安全提供 INA300 电源。

否则电池反插可能先损坏 LDO，且外部驱动 `SERVO_BUCK_IN` 时 LDO 可能形成未审计的反灌路径。

### 3.3 INA300 ALERT 与 LM74800 EN/UVLO 的电平接口

INA300 ALERT 是低侧开漏输出，不能默认直接接在电池分压节点上。需要明确：

- ALERT 上拉电源及其掉电默认状态；
- MASTER_ENABLE=OFF 时 EN/UVLO 的硬件拉低路径；
- ALERT 拉低时不会向 3.3 V AON 或 LM74800 分压节点反灌；
- 使用 NPN/NMOS wired-kill 后的 VIL/VIH、漏电和上电默认状态；
- INA300 LATCH 低电平清除时间满足 MASTER off→on 的复位时序。

推荐逻辑仍为：`MASTER_ENABLE=0 → EN/UVLO forced low, LATCH low`；`MASTER_ENABLE=1 → latch mode enabled`；过流后 `ALERT low → EN/UVLO forced low`。

### 3.4 TVS 与 30 V MOSFET

`SMBJ16A` 的 `VRWM=16 V`、`VC=26 V` 使它成为合理候选，但 `VC=26 V @ 23.1 A, 10/1000 µs` 不是所有板级瞬态的完整证明。还需核对：

- SMBJ16A exact suffix 和原厂资料；
- 电池线束/XT30/保险丝寄生 L 下的脉冲电流与能量；
- TVS 重复脉冲和热；
- `VDS=30 V` FET 的瞬态 avalanche/SOA 余量；
- TVS 在电池反接时是否会正向导通，及该行为是否安全。

### 3.5 5 mΩ shunt 的位置和功耗

Shunt 必须位于 INA300 两个输入的 Kelvin 取样点，且与两颗 FET、LM74800 的功率回路分开布线。其典型耗散为：

- 6 A：`0.18 W`；
- 7.5 A：`0.281 W`；
- 9 A：`0.405 W`。

需要 exact package、额定功率、TCR、铜箔散热和 40 °C 环境下温升资料；不能只写“1 W”而不核对降额曲线。

## 4. 与旧方案的比较

| 项目 | LM74800 + INA300 | LMX5069 + MX74610 |
| --- | --- | --- |
| 反接/背靠背驱动 | 一个控制器完成 | 两个控制器和额外 FET |
| UV/OV | LM74800 分压 | LMX 支路参数待闭合 |
| 过流 | INA300 快速比较器，外部关断 | LM5069 线性限流/TMR，SOA压力大 |
| reverse leakage | 方向上更有利，但需 exact system budget | MX74610 180 µA 已超过目标 |
| 成本 | 约 14.44 元核心 IC，待加小料 | 控制器数量、FET 和参数件更多 |
| 主要残余风险 | AON LDO反接、LM74800 reverse-current corner、wired-kill | PWR/SOA/TMR/TVS/OVLO 多项同时未闭合 |

## 5. 当前登记状态

`LM74800QDRRRQ1 + INA300AIDGSR` 现登记为 EDD-002 的 **preferred candidate / not accepted**。不应删除 LTC4368 历史计算，直到新候选完成以下资料核对：

1. LM74800 exact datasheet revision、pinout、EN/UVLO/OVLO corners、DGATE/HGATE default、reverse-current maximum/test condition；
2. INA300 exact datasheet revision、全温 `VOS`、ILIMIT source tolerance、LATCH/ALERT timing and reset；
3. TLV70433 reverse-input/reverse-current behavior，或替代 AON supply；
4. SMBJ16A exact datasheet；
5. 5 mΩ shunt exact MPN/package/TCR/power derating。

这些资料齐全后，才能把 UV/OV/overcurrent/reverse-current/default-off 写入一份完整可复算的 EDD-002 validation script，并决定是否将原 `LTC4368IMS-2` 从候选表中移除。
