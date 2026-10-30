# EDD-001 可插拔保险丝候选比较 v0.1（DRAFT 副本）

状态：**DRAFT / 非权威 / 未经 review**。不改动任何源文件，不宣告选择已冻结，不授权 schematic/layout/采购/测试。

日期：2026-08-30

Owner：Project maintainer（待审阅）

关联：
- [`../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md`](../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md) EDD-001
- [`EMPIRICAL_BOUND_SENSITIVITY_v0.1.md`](EMPIRICAL_BOUND_SENSITIVITY_v0.1.md)（短路电流与 breaking 敏感性）

## 1. Operator 决定记录（本副本依据）

- Operator 于 2026-08-30 明确：**不要 1206 板焊保险丝，改用可插拔（holder/socket）方案**，烧坏后可直接更换。
- 该决定取代 EDD-001 此前"`JKSEMI 12 100.10 / C355692` 1206 SMD"的选择，属于 EDD-001 正常工程决策（DIB-005 仅冻结 `≥32 VDC / ≥100 A DC breaking` 下限，不违反）。若要把更高 breaking 下限写死为硬要求，才需 Operator 修订 Design Input Freeze。
- Operator 于 2026-08-30 进一步决定（选项 A）：**breaking 目标 ≥250 A，形态为陶瓷填砂 5×20 + PCB 座**。理由：覆盖现实最坏短路 ~200 A + 余量；可插拔 holder 的陶瓷填砂保险丝 breaking 通常数百 A，提高 breaking 不显著增加成本。
- 已接受的残余风险：经验最坏角（新电芯 8 mΩ/节 + 紧回路）短路电流 ~309 A 超出 250 A 目标。该角为低概率，Operator 接受其为未验证残余风险，不以估算值伪装成已验证。

## 2. 需求汇总

| 需求 | 来源 | 数值 |
| --- | --- | --- |
| 额定电压 | DIB-005 / v0.3 §3.1 | `≥32 VDC` |
| DC breaking | DIB-005 / v0.3 §3.1 | `≥100 A`（下限） |
| DC breaking 目标 | Operator 决定（2026-08-30，选项 A） | **≥250 A**；覆盖现实最坏 ~200 A + 余量，最坏角 309 A 记为残余风险 |
| 额定电流 | EDD-001 | 10 A 级初始候选，最终按 6 A continuous / 8 A·1 s 协调 |
| 特性 | DIB-005 | fast-acting，8 A/1 s 不误断、catastrophic short 快速清除 |
| 可绕过性 | DIB-005 | 不可由 firmware / active controller 绕过（holder 天然满足） |
| 可更换性 | 本次 Operator 决定 | 可插拔，烧坏可直接换 |

## 3. 候选比较

| 候选 | 形态 | DC breaking（需以具体 datasheet 确认） | 额定 | 更换方式 | 板面积/高度 | 接触电阻(ASSUMPTION) | 关键风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ✅ 5×20 mm 陶瓷管 + PCB 座（SELECTED，Operator 决定） | 管状保险丝 + 焊板座子 | 高；陶瓷填砂常见数百~1500 A，**DC@32V 须单查** | 10 A F（快断）常见 | 手拔保险丝，座子焊板上 | ~22×10 mm，高 ~12 mm | 座子 ~5–15 mΩ | DC breaking 需确认；座子接触电阻进 6 A/8 A 路径 |
| 迷你刀片 + PCB 座 | 刀片 + 座子 | 很高；汽车级 32 VDC 常见 1000 A | 10 A 标准色 | 免工具手拔 | ~15×10 mm，高 ~20 mm | 座子 ~5–15 mΩ | 标准刀片偏慢，fast-acting 曲线需确认；高度较高 |
| 5×20 mm + 免焊夹 (clips) | 夹子 + 管状保险丝 | 同 5×20 | 同 5×20 | 手拔 | 更矮 | 夹子 ~10–20 mΩ | 夹子保持力（振动） |

说明：

- 陶瓷（填砂）分断能力优于玻璃管，catastrophic backup 优先陶瓷。
- "fast-acting"必须核对 time-current 曲线同时满足两端：8 A/1 s/≥10 s 不误断（no-nuisance），以及 ≥250 A 级短路快速清除。
- 上表 breaking 数值是行业典型范围（ASSUMPTION），**必须以具体 MPN 的原厂 datasheet 确认 DC breaking、fast-acting 曲线和 10 A 级额定**，选定前不写入选择。

## 4. 新增残余风险（并入 PVR）

1. **座子/夹子接触电阻**：新增 ~5–20 mΩ 进入 battery→buck 主电流路径。6 A 时 ~0.03–0.12 V、8 A 时 ~0.04–0.16 V 额外压降，且接触点温升需满足 DIB-002（ΔT≤30 °C）→ 并入 PVR-001/013（压降/热点）。
2. **振动/冲击保持**：Quad 是移动平台，保险丝与座子可能松脱或接触间歇 → 新增机械验证项，确认 holder 保持力与反复插拔后接触电阻稳定。
3. **高度/外壳干涉**：管状或刀片 holder 高于 1206，需在 layout/机械阶段核对净空。

## 5. 敏感性复述（为什么目标定 ≥250 A）

沿用 [`verify_empirical_bounds.py`](verify_empirical_bounds.py) 的电池短路边界（含保险丝内阻后）：

| 情形 | 预期短路电流 | 结论 |
| --- | --- | --- |
| 新电芯 + 紧回路 | 309 A | 要求 breaking ≥ 此值 |
| 典型 | 137 A | 150 A 已够 |
| 老化/低温 + 长回路 | 75 A | 150 A 已够 |

可插拔方案的 breaking 通常在数百 A 以上，足以覆盖现实最坏 ~200 A。**决定目标 ≥250 A**（现实最坏 + 25% 余量）；最坏角 309 A 超出 250 A，作为 Operator 已接受的残余风险记录。选定 MPN 后仍须以其实际 DC breaking 复核。

## 6. 待 Operator 决定 / 待核对

1. ~~目标 breaking 定多少~~ → **已定：≥250 A**（Operator 决定，2026-08-30）；
2. ~~候选形态~~ → **已定：陶瓷填砂 5×20 + PCB 座**；
3. exact MPN + 原厂 datasheet：确认 DC breaking（≥250 A）、fast-acting 曲线、10 A 级额定、座子接触电阻 —— 待核对；
4. 是否把"≥250 A DC breaking"写进 Design Input Freeze 作为硬要求 —— 待 Operator 决定（不写则仍属 EDD-001 正常决策）。

## 7. 边界声明

本副本所有 breaking/接触电阻数值均为行业典型范围（ASSUMPTION），非原厂保证、非实测；不得以本副本代替具体 MPN 的原厂资料核对。合入权威记录前须经 Operator 审阅。

## 8. 变更记录（Change log）

| 日期 | 变更 | 由 |
| --- | --- | --- |
| 2026-08-30 | 初稿：候选比较，breaking 目标"建议 ≥300 A"，形态未定 | Agent（待 Operator 审阅） |
| 2026-08-30 | Operator 决定选项 A：breaking 目标 **≥250 A**、形态 **陶瓷填砂 5×20 + PCB 座**；同步更新 §1/§2/§3/§5/§6；最坏角 309 A 记为已接受残余风险 | Project maintainer |
