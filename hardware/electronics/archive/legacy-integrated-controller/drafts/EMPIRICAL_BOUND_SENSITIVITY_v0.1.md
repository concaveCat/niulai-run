# 参数经验边界与敏感性估算 v0.1（DRAFT 副本）

状态：**DRAFT / 非权威 / 未经 review**。本文件是 Circuit Design 的辅助估算副本，不是冻结文件，不改动任何源文件，不宣告任何 PVR/EDD CLOSED，也不授权 schematic、layout、采购或实体测试。

日期：2026-08-30

Owner：Project maintainer（待审阅）

关联源文档：
- [`../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md`](../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md)
- [`../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md`](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md)（当前唯一权威电气设计输入）

可复现脚本：[`verify_empirical_bounds.py`](verify_empirical_bounds.py)，运行命令见第 4 节。

## 0. 目的与边界

样板机验证只能在制板之后发生；要求"先实测再设计"会构成死循环。因此本文对当前无法实测的参数采用**经验边界（ASSUMPTION）+ 敏感性分析**，让纸面 worst-case 分析得以推进到可制版的保守程度。

严格区分：

- **权威冻结值**：来自 Design Input Freeze v0.6 及其规范性纳入条款；
- **原厂保证值**：来自已登记的原厂资料；
- **经验边界（ASSUMPTION）**：本文自行给出的行业典型范围，**不是** manufacturer guaranteed、**不是**实测值；
- **敏感性**：说明参数在边界两端变化时结论如何变化，避免把单一估算值当成精确值。

本文所有 ASSUMPTION 都不会写入权威 Freeze，除非 Operator 明确接受且仍以"假设边界"名义记录。未验证项仍保持 PVR，不因本估算而关闭。

## 1. 估算与敏感性结论

### 1.1 电池内阻与短路电流（影响 fuse breaking 容量）

- 权威输入：battery-entry physical fuse 额定 `≥32 VDC`、DC breaking `≥100 A`（DIB-005）；已选 `C355692 / 10 A / 32 VDC / 150 A DC breaking`（EDD-001）。
- ASSUMPTION：3S 650 mAh 航模电芯单体内阻 8/20/40 mΩ（fresh / typical / aged-cold），短路回路额外电阻（XT30 + PCB + 焊点）10/40 mΩ。
- 注意：**保险丝不是限流器，而是开断器**。它是串联元件（冷阻 ~6.8 mΩ），熔断前几乎不阻挡电流；其自身电阻已计入下面的回路电阻。"预期短路电流"是保险丝必须**先承受再分断**的电流，因此 breaking capacity 必须覆盖它，而不是指望它挡住电流。

敏感性结果：

| 情形 | 回路电阻(含保险丝) | 预期短路电流 | 熔断时间(20 A²s 估算) | 对 150 A breaking 的余量 |
| --- | --- | --- | --- | --- |
| 低阻（新电芯 + 短回路） | 40.8 mΩ | 309 A | 0.21 ms | **−159 A（不足）** |
| 典型 | 91.8 mΩ | 137 A | 1.07 ms | +13 A（勉强） |
| 高阻（老化/低温 + 长回路） | 166.8 mΩ | 75 A | 3.52 ms | +74 A（充足） |

**设计结论**：150 A DC breaking 只覆盖高阻端；低阻新电芯 + 紧回路的最坏预期短路电流可能超过它。Operator 于 2026-08-30 决定（选项 A）：放弃 1206，改用可插拔保险丝，breaking 目标 **≥250 A**、形态 **陶瓷填砂 5×20 + PCB 座**（见 [`EDD001_FUSE_CANDIDATES_v0.1.md`](EDD001_FUSE_CANDIDATES_v0.1.md)）。该目标覆盖现实最坏 ~200 A + 余量；最坏角 309 A 超出 250 A，作为 Operator 已接受的残余风险记录（低概率，不伪装成已验证）。

该项对应 Open Issues 的 EDD-001 残余风险（"电池真实短路电流仍按 PVR 验证"），本文不关闭它。

### 1.2 Fuse 无扰断与热（C355692, 10 A）

- 40 °C 图形降额约 `0.97` → 图形筛选额定约 `9.7 A`；8 A/1 s 波形是其 `0.825` 倍 → 对 1 s 误断有正向证据（比 GLM 草案的 I²t 比较更强，因 20 A²s 是典型 10×In 值而非 no-blow 门槛）。
- 6 A 导通损耗约 `0.245 W`、8 A 约 `0.435 W`（典型冷阻 6.8 mΩ）。熔断后需焊接维修，最大热阻/曲线 tolerance 未给出，6 A 温升与 8 A 重复波形仍为 PVR-001/012。

### 1.3 舵机堵转交叉验证（MG90S）

- ASSUMPTION：MG90S 单只堵转 0.7–1.0 A @5–6 V；8 只同时堵转 ≈ `5.6–8.0 A`。
- 最坏 8 A = 冻结连续值 6 A 的 `1.33×`、正好压到冻结瞬态 8 A 上限。
- **设计结论**：冻结 6 A/8 A 包络是物理上可达到的（不是虚高），因此 firmware + active protection **不得允许持续 8 路堵转**；超包络必须限流/关断。这印证了 DIB-001/006 包络的必要性，不产生新要求。

### 1.4 ST7789 供电

- ASSUMPTION：控制器 10–20 mA + 背光 20–80 mA → 模块合计 `30–100 mA`。
- 冻结预算 `0.5 A`（经 0.75 A PTC）在最坏估算下仍有 `5×` 余量 → **供电预算充足**。
- `BLK` 由 TCA9539 P12 作低速逻辑使能（走 GPIO），背光电流走模块自身 VCC（受保护 3V3 支路），故模块内部接法不影响板级设计；仅在拿到具体 SKU 后确认 BLK 极性/是否需串阻即可（原 PVR-010 的"供电"部分可视为已由预算覆盖，但 BLK 回灌/极性仍保留至到货确认）。

### 1.5 HAL49ESO 3.3 V 前端（已有，引用）

- 已由 [`verify_hall_adc_frontend.py`](../calculations/verify_hall_adc_frontend.py) 完成：最坏 3.6 V Hall 输出 → ADC `2.838 V`，headroom `62.2 mV`，`fc=67.3–82.2 Hz`。
- ASSUMPTION：Hall 输出近似 ratiometric（3.3 V 时按比例缩小），但原厂仅在 5 V 给出完整传递参数 → 实际零点/灵敏度仍为 PVR-008，本文不猜具体数值。

### 1.6 SY8388ARHC 损耗与热（PVR-015 相关）

- 原厂 typical `RDS(on)` 20/10 mΩ；ASSUMPTION：100 °C 结温乘 1.5–1.7；开关 tr+tf 合计 10–30 ns；θJA=33 °C/W（原厂 8.5×8.5 cm 四层 2 oz eval board）。
- 结果（最坏角，12.6 V）：

| 工况 | 导通损耗 | 开关损耗 | 合计 | 对照 P_allow=2.58 W |
| --- | --- | --- | --- | --- |
| 6 A continuous | 0.90 W | 0.45–1.36 W | **2.26 W** | OK |
| 8 A / 1 s 最坏 | 1.61 W | 0.60–1.81 W | **3.42 W** | MARGINAL |
| 8 A/1 s 按 10% 占空比平均 | — | — | **2.38 W** | OK |

**设计结论**：6 A continuous 热余量成立；8 A/1 s 单次最坏角在开关损耗假设下偏临界，但其 10% 占空比平均功率仍在界内，且 tr/tf 假设偏保守。结论：**6 A 连续热成立，8 A 瞬态热保持 PVR-015**，不因本估算关闭。

## 2. 对 Open Issues 的映射（本副本能推进什么）

| Open Issues 项 | 本副本结论 | 状态变化 |
| --- | --- | --- |
| EDD-001（fuse） | 无扰断证据 + breaking 容量敏感性 | 仍 PARTIAL；breaking 待决（见 1.1） |
| EDD-002（电池保护） | 短路电流边界供 SOA/ILIM 使用 | 仍 PARTIAL；不因此关闭 |
| EDD-008（common servo 保护） | 堵转交叉验证确认 6 A/8 A 包络物理可达 | 输入更充分；仍 PENDING |
| PVR-010（ST7789） | 0.5 A 预算 5× 余量，BLK 走 GPIO | 供电部分可视为覆盖；BLK 极性到货确认 |
| PVR-015（板载 buck） | 6 A 热成立、8 A 瞬态临界 | 仍 OPEN（样机验证） |
| PVR-001/012（fuse 热/clearing） | 给出损耗与短路电流边界 | 仍 OPEN（样机验证） |

本副本**不关闭任何 EDD/PVR**，只为后续 EDD 计算提供已标注假设的输入边界。

## 3. 仍未解决、需 Operator 决定或样机确认的项

1. **fuse breaking 容量**（1.1）：已由 Operator 决定为 ≥250 A + 陶瓷 5×20 可插拔；剩余为选定 MPN 的 DC breaking/fast-acting 曲线核对，以及最坏角 309 A 的残余风险。
2. 电池真实内阻/短路电流：本文是经验边界，实测仍归 PVR。
3. 舵机持续 8 路堵转：firmware 策略须写入后续安全默认说明。
4. HAL 3.3 V 实际传递函数、SY8388 8 A 瞬态热、fuse 重复波形温升：仍归 PVR-008/015/012。

## 4. 可复现命令

```sh
python3 hardware/electronics/drafts/verify_empirical_bounds.py
```

所有输入常量和 ASSUMPTION 都在脚本头部集中声明；任何结论改动都应从改常量/公式并重跑开始。

## 5. 本副本的边界声明

- 不改动任何源文件；本目录 `drafts/` 为独立工作区。
- 所有经验值均为 ASSUMPTION，非 manufacturer guaranteed、非实测；不得以"agent 已检查"或"基本能跑"替代后续验证。
- 本副本不授权 schematic capture、ERC、netlist、PCB layout、采购、下单或实体测试。
- 合入权威记录前必须经维护者审阅，并按 CONTRIBUTING.md 区分事实/假设/决定。
