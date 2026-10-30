# EDD-002 候选复核 v0.2：LM74800 + INA300 修订

状态：**Preferred candidate / review open**。本版记录 Operator 对 AON LDO、TVS、分流器和漏电结论的修订；不改变 `PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md`，不授权 schematic、ERC、netlist、PCB 或实体测试。

## 1. 当前候选组合

```text
VBAT_RAW
  → 10 A / 5×20 ceramic fuse + holder
  → FUSE_OUT_RAW
  → SMCJ15CA (bidirectional TVS to GND)
  → 5 mΩ Kelvin shunt
  → LM74800-controlled 2×HD303N080SG
  → SERVO_BUCK_IN
```

辅助保护与控制：

- `LM74800QDRRRQ1 / C3215600`：反接、背靠背 FET 驱动、UV/OV、enable、ideal-diode reverse control；
- `INA300AIDGSR / C529558`：50 µs overcurrent comparator，`LATCH=MASTER_ENABLE`，ALERT 触发硬件 EN/UVLO kill；
- `MCSF2512DR005Q9 / C49437818`：5.000 mΩ、±0.5%、±50 ppm/°C、2512、3 W@terminal70 °C；
- `TPS70933DBVR / C89347`：AON 3.3 V regulator；
- `BAS116,215 / C48502`：AON LDO 输入串联反接隔离；
- `SMCJ15CA / C151273`：当前 TVS 首选，`SMBJ15CA / C78809` 为小封装备选；
- `QKILL=2N7002,215 / C65189`，PNP driver=`PMBT3906,215 / C8670`：仅为 wired-kill 候选，阻值和具体连接仍需闭合。

## 2. 本版已接受的修正

### 2.1 删除 SMBJ16A

单向 `SMBJ16A` 直接跨电池入口在反接时会正向导通并形成保险丝 crowbar，因此不能用于该位置。双向 `SMCJ15CA`/`SMBJ15CA` 在 ±12.6 V 正常电池范围内不会产生该正向导通路径。

### 2.2 AON LDO 由 TLV70433 改为 TPS70933

`TPS70933DBVR` 的推荐输入上限 30 V，高于 `SMCJ15CA` 给定 24.4 V clamp；因此解决了原先 `SMBJ16A + TLV70433` 的 26 V 对 24 V 冲突。`BAS116` 必须按 `FUSE_OUT_RAW → anode BAS116 / cathode TPS709 VIN` 方向连接，并核对反向耐压、正向压降、LDO 输入电容和掉电路径。

该结论只证明额定值方向成立；仍需检查 TVS 实际寄生过冲是否可能把 LDO 输入推高到 30 V 以上。

### 2.3 INA300 nominal/corner screen

`5 mΩ + 2.74 kΩ` 的名义过流阈值约 11 A。若采用给出的全温边界：

- `ILIMIT` 约 ±0.75%；
- `RLIMIT` ±1%；
- `RSENSE` ±0.5%，TCR ±50 ppm/°C；
- `VOS` 按 50 µs 模式 `±500 µV` 加 25→125 °C 的 `0.5 µV/°C` 漂移，形成工程上界 `±550 µV`；

则 rising trip 的工程筛选约为 `10.55–11.38 A`。这个范围覆盖 9 A 总输入包络且低于 12 A 目标上限。最终文件仍应区分“datasheet guaranteed individual limits”和“将多个最大值相加形成的 engineering bound”。

## 3. 必须保持开放的两项

### 3.1 LM74800 正常正输入下的稳态反灌

LM74800 的 closed-loop regulation/“zero DC reverse current”是有利的功能描述，但当前资料没有给出与要求完全同条件的 `SERVO_BUCK_IN → VBAT_RAW` steady reverse-current guaranteed maximum。反接输入下的 `I(C)` 或 `IREV` 参数不能代替该证明。

分类：

- 若 ≤100 µA 允许作为生产/样机 qualification criterion：保留为 EDD-002 closure + PVR，需定义电压、温度、MASTER 状态和测量方法；
- 若要求必须由 datasheet guaranteed max 证明：它是 `COMPONENT_DATA_BLOCKER`，LM74800 不能直接冻结。

### 3.2 TVS 重复瞬态

`SMCJ15CA` 的 24.4 V clamp/61.5 A/10×1000 µs 是器件规定波形下的 rating，不是任意重复浪涌的系统保证。必须先定义：源电压、源阻抗、脉宽、单次能量、重复周期和最大次数。

在此之前：

- `SMCJ15CA` 是比 `SMBJ15CA` 更合理的封装/脉冲能力选择；
- 不能关闭 TVS 的重复寿命、FET 30 V 瞬态余量或 TPS709 30 V 输入余量；
- 若系统不定义外部浪涌边界，该缺失按 `DESIGN_INPUT_BLOCKER` 处理；若接受明确的纸面边界并把实测留给样机，则可转为 PVR。

## 4. 连接和默认状态注意事项

1. `INA300 ALERT` 为 active-low open-drain，不能直接并到 LM74800 UV 分压。必须用独立低侧 wired-kill，并分别保证 `MASTER_ENABLE=0` 与 `ALERT=0` 都能将 EN/UVLO 拉低。
2. `MASTER_ENABLE` 必须来自 AON/实体开关域，不得由 `SERVO_BUCK_IN` 上电后的 MCU 产生，否则会形成启动死锁。
3. `LATCH=MASTER_ENABLE` 只有在 MASTER off 时间满足至少 20 µs 且过流已消失时才清除；系统建议保持 ≥1 ms。
4. AON LDO 位于保险丝之后、主 FET 之前，以便主路径 latch-off 后 INA300 仍保持锁存；BAS116 方向必须防止反接和外部 SERVO_BUCK_IN 驱动造成 AON 反灌。
5. 旧的 `verify_edd002_battery_protection.py` 针对 LTC4368，不能作为本候选的证明；必须另建 LM74800/INA300 corner script。

## 5. 当前状态

- `DESIGN_INPUT_BLOCKER`：现有 v0.6 输入没有因本候选自动修改；TVS 系统波形是否需要新增输入定义待决定；
- `COMPONENT_DATA_BLOCKER`：LM74800 reverse-current 同条件 maximum 尚未确认；若采用样机 qualification，则可不升级为永久 CDB；
- EDD-002：**preferred candidate / not accepted**；
- Circuit Design Freeze：**不可提出**。

## 6. 下一步

1. 取得并记录 LM74800、INA300、TPS70933、BAS116、SMCJ15CA、MCSF2512DR005Q9 的 exact manufacturer document metadata；
2. 建立新候选专用计算：UV/OV、OCP 全 corner、AON LDO 输入/反接、wired-kill 阈值和 leakage budget；
3. 定义 TVS 系统瞬态 envelope，或由 Operator 明确将其作为 PVR 边界；
4. 完成 wired-kill 原理级连接表和 default/fault truth table；
5. 通过上述 review 后，再把 LTC4368 从 EDD-002 的活动候选降级为历史比较基线。
