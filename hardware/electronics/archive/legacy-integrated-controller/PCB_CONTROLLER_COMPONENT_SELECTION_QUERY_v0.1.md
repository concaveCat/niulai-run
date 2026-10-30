# 牛来快跑 PCB Controller Rev A — Component Selection Query v0.1

状态：**Operator / 嘉立创代查输入；不是 BOM，也不是 Circuit Design Freeze**

日期：2026-08-30

关联登记册：[`PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md`](PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md)

## 1. 使用方法

电路设计先给出功能、连接和不可缺少的器件行为；Operator 再从嘉立创可装配范围返回少量候选，并负责取得相应 manufacturer datasheet。收到候选和资料后，设计者只审阅 Operator 提供的 exact MPN、商品条目与原厂资料，完成 exact MPN 选择；按 Operator 当前指示，受影响的技术参数计算继续暂停到选型范围完成后。本阶段不再由设计者自行联网搜索 datasheet。

每个候选请尽量返回：

- 嘉立创/LCSC 商品链接与物料编号；
- exact manufacturer、exact MPN 和 package；
- 基础库/扩展库、当前可贴装状态和库存；
- 约 5–10 片数量下的单价；
- 页面所链接的 manufacturer datasheet；若只有商城参数或第三方 PDF，请明确注明。

商城参数只用于寻找候选，关键 pinout、阈值、额定值和故障行为仍由 Operator 提供的原厂资料确认。未提供可靠资料时保持 `PENDING` 或登记/保留 `COMPONENT_DATA_BLOCKER`，不得猜测。

## 2. 第一批：现在可以查询的低压器件

这些器件的连接架构不会被当前主功率 blocker 推翻。候选不需要一次找很多；每项 1–3 个即可。

| Query ID | 功能                                    | 必须条件                                                                                                          | 封装/装配偏好                                                     | 已查到但未锁定的参考候选                                | 请返回                                                                                                                                                                                                                                                                                                                                                                                                                           |
| -------- | ------------------------------------- | ------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------- | ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| LVQ-01   | ESP32 3V3 reset supervisor            | active-low open-drain reset；监测 3V3；falling threshold 高于 3.0 V 且低于最终 rail 下限；power-on delay；manual-reset input | SOT-23-6 或相近、单面 SMT                                         | `TPS3808G33DBVR`                            | TPS3808G33DBVR-TP；\|   \|   \|<br>\|---\|---\|<br>\|商品目录\|监控和复位芯片\|<br>\|\|芯片类型\|电压监控器\|<br>\|\|输出类型\|开漏\|<br>\|\|工作电压\|1.7V~6.5V\|<br>\|\|复位有效电平\|低电平有效\|<br>\|\|阈值电压\|3.07V\|；- 具有可调延迟时间的上电复位发生器：1.25ms至10s。<br>- 极低静态电流：典型值2.8uA。<br>- 高阈值精度：典型值0.5%。<br>- 提供针对0.9V至5V标准电压轨的固定阈值电压，以及可低至0.4V的可调电压选项。<br>- 手动复位输入。<br>- 开漏复位输出。<br>- 工作温度范围：-40℃至125℃。<br>- 封装：SOT23-6L (TPS3808XXXDBVR-TP)， DFN2x2-6L (TPS3808xXxDRVR-TP)。 |
| LVQ-02   | USB DATA Type-C receptacle            | USB 2.0 16-pin receptacle；CC1/CC2、D+/D−和 VBUS/GND contacts 完整；原厂 drawing/footprint 可取得                        | 水平 top-mount；优先 SMT signal + THT shell stakes；适合 1.6 mm PCB | GCT `USB4105-GF-A-060` 仅作 footprint/资料完整性参考 | 嘉立创C393939，\|   \|   \|   \|<br>\|---\|---\|---\|<br>\|\|商品目录\|USB连接器\|<br>\|\|连接器类型\|Type-C\|<br>\|\|协议标准\|-\|<br>\|\|公母\|母\|<br>\|\|安装方式\|卧贴\|<br>\|\|触点数量\|16P\|<br>\|\|端口数量\|1\|<br>\|\|额定电流 - 电源\|3A\|                                                                                                                                                                                                                   |
| LVQ-03   | USB DATA/CC ESD array                 | 4 channels；working standoff ≥5.5 V；D+/D−通道 capacitance ≤1 pF；有 IEC 61000-4-2 规格                               | 可可靠贴装的 SOT/DFN/USON；避免 WLP                                  | `TPD4E05U06DQAR`                            | 已经是最低成本                                                                                                                                                                                                                                                                                                                                                                                                                       |
| LVQ-04   | USB VBUS presence detector transistor | 通用 NPN；VCEO ≥20 V；在亚毫安 collector current 有明确 gain 数据                                                          | SOT-23                                                      | `MMBT3904LT1G`                              | 已经是最低成本                                                                                                                                                                                                                                                                                                                                                                                                                       |
| LVQ-05   | I2S Class-D amplifier                 | 必须为 `MAX98357A` 功能；standard I2S；封装必须在目标嘉立创装配服务中可贴装                                                  | 权威输入仅**优先** TQFN-16、避免 0.4 mm WLP；WLP 不是禁止项                   | `MAX98357AETE+T`（TQFN）或 `MAX98357AEWL+T`（WLP） | 可选    MAX98357AEWL+T，\|   \|   \|<br>\|---\|---\|<br>\|功放类型\|D类功放\|<br>\|\|扬声器通道数\|单声道；双声道\|<br>\|\|输出功率\|3.2W×1@4Ω；0.93W×1@8Ω；1.8W×1@8Ω\|<br>\|\|总谐波失真+噪声(THD+N)\|0.015%\|<br>\|\|静态电流(Iq)\|3.35mA\|<br>\|\|电源纹波抑制比(PSRR)\|77dB\|<br>\|\|效率\|92%\|<br>\|\|工作电压\|2.5V~5.5V\|<br>\|\|工作温度\|-40℃~+85℃\|。 |
| LVQ-06   | 四路足端输入 ESD                            | 4 channels；3.3 V signal compatible；working standoff ≥5.5 V；IEC 61000-4-2 数据                                   | 易贴装的 SOT/DFN/USON；capacitance 不敏感                           | 可与 LVQ-03 共料，否则另选普通四通道 ESD                  | 双向ESD只需￥0.04/个，参考    H5VL10B。\|   \|   \|   \|<br>\|---\|---\|---\|<br>\|\|反向电流(Ir)\|100nA\|<br>\|\|通道数\|单路\|<br>\|\|工作温度\|-55℃~+125℃\|<br>\|\|防护等级\|IEC 61000-4-2；IEC 61000-4-5\|<br>\|\|类型\|ESD\|<br>\|\|Cj-结电容\|15pF\|\|   \|   \|<br>\|---\|---\|<br>\|商品目录\|静电和浪涌保护(TVS/ESD)\|<br>\|\|极性\|双向\|<br>\|\|反向截止电压(Vrwm)\|5V\|<br>\|\|钳位电压\|10V\|<br>\|\|峰值脉冲电流(Ipp)\|5A\|<br>\|\|峰值脉冲功率(Ppp)\|50W\|<br>\|\|击穿电压(VBR)\|5.6V\|。 |
| LVQ-07   | 足端开关板端 2P connector                   | 2 pins、`GND/SW`；有原厂 drawing 和明确 mating family；适合低电流线束                                                         | 优先 THT vertical、2.00–2.54 mm pitch；避免与舵机 3P 混插              | 无                                           | # PZ2.0-1x2PZZ-L8.7只需要0.0349元/个                                                                                                                                                                                                                                                                                                                                                                                               |

### 2.1 第一批返回结果验证（2026-08-30）

本表只验证候选身份、资料来源、功能和封装方向，不是采购锁定。本轮按 Operator 要求不运行新的阈值、时序、热或最坏情况参数计算。

| Query ID | 验证状态 | 规范化后的候选与证据 | 处理结论 / 仍需 Operator 补充 |
| --- | --- | --- | --- |
| LVQ-01 | **DATA VERIFIED / INPUT AVAILABLE / CALC DEFERRED** | 嘉立创 `C51886189`；TECH PUBLIC（台舟）`TPS3808G33DBVR-TP`；SOT-23-6L。Operator 提供的 10-page TECH PUBLIC 原厂正文包含 exact orderable row、pinout、full-temperature electrical table、delay truth/timing 和 package drawing；SOT-23 pin 1=`RESET_N`、2=GND、3=`MR_N`、4=CT、5=SENSE、6=VDD。来源：[LCSC item](https://www.lcsc.com/product-detail/C51886189.html)；本地审阅副本 `C51886189_监控和复位芯片_TPS3808G33DBVR-TP_规格书_WJ1762677.PDF`，SHA-256 `182b6e242fdbd464385b13a5f0e92fcefc5de8a6af0ec972a53bbc1c609e5d0a`。 | 自身组件资料已足够，不再借用 TI datasheet。DIB-004 已由 v0.3 关闭，exact rail/threshold compatibility 现在可以计算；按 Operator 指示仍暂缓运行参数计算，不要求扩大候选范围。 |
| LVQ-02 | **VERIFIED CANDIDATE** | 嘉立创 `C393939`；SHOU HAN（首韩）`TYPE-C16PIN`；16P horizontal Type-C receptacle，SMD signal、四脚插板。LCSC 托管的原厂规格书正文包含 exact manufacturer/part number、机械图和额定值。来源：[LCSC](https://www.lcsc.com/product-detail/C393939.html)、[manufacturer document hosted by LCSC](https://www.lcsc.com/datasheet/lcsc_datasheet_1912112237_SHOU-HAN-TYPE-C16PIN_C393939.pdf)。 | 满足 USB DATA 的最小连接器需求，纳入 EDD-011；后续只需在 Circuit Design Freeze 前复核 drawing 的 pin numbering/view orientation，不要求继续找更贵器件。 |
| LVQ-03 | **VERIFIED / SELECTED** | Operator 已确认 TI 原厂 `TPD4E05U06DQAR`、嘉立创 `C138714`；USON-10、4 channel、5.5 V working standoff、0.5 pF，并有 IEC 61000-4-2 规格。来源：[TI datasheet Rev. O](https://www.ti.com/lit/ds/symlink/tpd4e05u06.pdf)、[JLC C138714](https://jlcpcb.com/partdetail/TexasInstruments-TPD4E05U06DQAR/C138714)。 | exact manufacturer/item ambiguity 已关闭，纳入 EDD-011。 |
| LVQ-04 | **VERIFIED CANDIDATE** | 嘉立创 `C81464`；onsemi `MMBT3904LT1G`；SOT-23，40 V/200 mA NPN；原厂资料给出 pinout 和低电流 gain 数据。来源：[LCSC](https://www.lcsc.com/product-detail/C81464.html)、[onsemi datasheet Rev. 14](https://www.onsemi.com/pdf/datasheet/mmbt3904lt1-d.pdf)。 | 满足 USB VBUS presence detector，纳入 EDD-011；无需继续优化该器件。 |
| LVQ-05 | **VERIFIED / SELECTED** | Analog Devices/Maxim `MAX98357AEWL+T`；嘉立创 `C2682619`；扩展库，9-bump WLP（嘉立创显示 `BGA-9 (1.3 × 1.3)`），2 块报价约 ¥3.01/块。Operator 提供的 manufacturer datasheet Rev. 11 明确列出 exact ordering row、WLP bump table 和 12 dB no-via connection。来源：[LCSC item](https://www.lcsc.com/product-detail/C2682619.html)；本地审阅副本 `C2682619_15CCD654F2C559A2FC5FDCA8EA0D4E6D(1).pdf`，SHA-256 `506fc7f9fed5c8b4f3eaa6d812b6cbdc23f56afb6a092c73a5ad8d8f6674f189`。 | 成本可接受，按 Operator 倾向正式选择 WLP，不再查询 TQFN。0.4 mm WLP 的 land pattern、单面贴装和首板检查作为后续 layout constraint / prototype risk，不改变电气功能。 |
| LVQ-06 | **VERIFIED CANDIDATE** | 嘉立创 `C7420372`；R+O / Zhuhai Hongjiacheng `H5VL10B`；单路双向、DFN1006-2L、5 V VRWM、IEC 61000-4-2。来源：[LCSC](https://www.lcsc.com/product-detail/C7420372.html)、[JLC exact item](https://jlcpcb.com/partdetail/H5VL10B/C7420372)。 | 对最高 3.3 V 的慢速足端输入，5 V VRWM 已足够；原先 5.5 V 条件是设计者预筛条件而非冻结要求，现收敛为 5.0 V。选择每路一颗、共四颗，不为共料强制使用昂贵四通道高速阵列。 |
| LVQ-07 | **CLOSED — excluded from exact electrical selection** | Operator 明确 `C22438285` 只是通用排针，其 exact MPN 无须进入电气/网表器件选型。 | Circuit Design 只保留四组足端电气端点，每组固定为 `GND/SW`；排针、直接焊线或等价机械实现留给后续装配/PCB 选择，不再追踪 exact header MPN。 |

本轮已收敛：LVQ-02、LVQ-03、LVQ-04、LVQ-05、LVQ-06；LVQ-07 从 exact electrical part selection 中移除。LVQ-01 的 exact MPN/资料和 DIB-004 输入均已收敛，只剩按指示暂缓的 threshold/rail compatibility 计算，不要求扩大候选范围。

### 2.2 Operator 补充的连接器、开关和 UBEC 结果

| Item | 状态 | 审阅结论 / 下一步 |
| --- | --- | --- |
| Speaker `ZX-MX1.25-2PZZ / C7430458` | **VERIFIED / CDB-002 CLOSED** | Operator 提供的 one-page drawing 正文包含 exact MPN、1.25 mm/2P 尺寸表、PCB layout、1 A/125 V、20 mΩ、材料和 version 2.0。Circuit Design 定义 manufacturer drawing 的 PCB-layout view 左孔为 pin 1=`SPK+`、右孔为 pin 2=`SPK-`；旁边仍保留直焊孔。本地副本 SHA-256 `11c9a5c13c109ae58d8f5f8564db1dad85cd9f565ffda8a947908210c40a47a7`。 |
| Battery XT30 | **VERIFIED / SELECTED** | 选择 Changzhou Amass `XT30UPB30-M.G.* / C431092` vertical PCB male。所给 3-page manufacturer drawing 含 exact family row、15 A rated/30 A instantaneous、尺寸及 2 × Ø1.90 mm / 5.00 mm pitch PCB mounting；SHA-256 `02372b47...ca5952`。`XT30U-M.N.B / C19268019` 的 D-1542/2021V1 文档确认 15 A family 和外形，但所给页没有完整 PCB drill pattern，故仅作未选替代；SHA-256 `50057652...070f4`。 |
| `VBAT_SWITCHED` 与 `UBEC_6V` termination | **EXACT PAIR SELECTED / ANALYSIS DEFERRED** | 选择同厂 KANGNEX/Wanjie 配对：板端 `WJ2EDGVC-5.08-02P-14-00A / C8445`，线端螺钉插头 `WJ2EDGK-5.08-02P-14-00A / C71370`；输入、输出各一套。原厂 `WJ2EDGK-5.08` 页面明确把 `WJ2EDGVC-5.08` 列为 adapted product；exact plug drawing Rev. A（2024-03-10）给出 10 A UL / 15 A IEC、300/320 V、24–12 AWG、2.5 mm²、7–8 mm strip、0.4 N·m。C8445 JLC 页给出 THT、5.08 mm、10 A、300 V、−40…+105 °C 和 snap-on/closed socket。普通 2.54 mm receiver/Dupont 已排除；如 UBEC 尾线带小端子则装配时剪除、剥线并优先加 bootlace ferrule，再锁入 C71370。固定针序：输入 pin 1=`VBAT_SWITCHED`/2=`GND`，输出 pin 1=`UBEC_6V`/2=`GND`。`KF2EDGVG-5.08-2P / C6392955` 与 `JL396V-39602G01 / C2913357` 都只是其他厂商板端半件，未提供同厂配对插头，故不选。按 Operator 指示，contact/thermal 与实际线径复核暂缓。 |
| `MASTER_ENABLE` switch | **VERIFIED / SELECTED / CDB-003 CLOSED** | 选择 `Bossie BX-BD12D07WC5 / C49447025`。Operator 提供的 Bossie `BX-BD12D07WCN`, Rev. A 原厂图纸明确系列中 `L=5.0 mm` 对应 WC5，并给出 body、travel、terminal/mounting geometry、center-common SPDT schematic、0.3 A/50 V 和 5,000 cycles。厂家未印焊脚编号，因此 Circuit Design 以图纸三脚正视图从左到右自定义 pin 1=throw 1、pin 2=common、pin 3=throw 2；该编号是项目 convention。`BX-BD12D07WC7 / C49447027` 仅保留长柄机械替代；`SS-12D02-G040 / C2876023` 不选。PDF SHA-256 `4b0c952aaacb9304fb07f43fa03521a98fa42a059b9de437dc2a46eed3de929e`。 |
| ESTOP termination | **GENERIC ACCEPTED** | 普通 2P 或两根直焊线足够；NC loop 的断线/拔出会进入安全停机。只固定 `ESTOP_LOOP_A/B` 电气端点，不要求昂贵安全连接器或 exact header MPN。 |
| YSIDO UBEC listing | **CDB-001 CLOSED BY v0.2 REVISION** | Operator 已明确不再查询资料；6 A continuous / 8 A short 和未知保护行为均降为 prototype validation target，不作为 manufacturer guarantee。主板路径设计目标不变，实测要求进入 PVR-002/PVR-014。 |

### 2.3 PWM engine 成本替代（v0.4 Accepted）

Operator 已明确授权 `CH32V003F4P6 / C5187096 + Nexperia 74HC244PW,118 / C5623` 取代 PCA9685。两份 Operator-provided manufacturer PDF 已审阅：

- WCH `CH32V003` datasheet V1.8，PDF metadata 2025-10-17，SHA-256 `2c05b25a3ef9269ca3685db09de05a2b31c650e1b389b0cecc735d4176985b91`。F4P6=TSSOP20、2.7–5.5 V、18 GPIO、TIM1/TIM2 各四通道、I²C 支持 master/slave、standard/fast mode、7/10-bit addressing，并具有 SWIO、NRST、programmable voltage detector 和 IWDG/WWDG。
- Nexperia `74HC244PW,118` product data sheet Rev. 7 / 2021-07-27，SHA-256 `7d5786046ad334ebf2072ef866e180b2464a12c3235ec5d6321e169e806044e7`。Exact HC variant supports VCC 2.0–6.0 V；PW=TSSOP20/SOT360-1；pin 1/19 为两个 active-low OE；OE high 时输出 high-Z。不得换成只保证 4.5–5.5 V 的 HCT variant。

已证明的无冲突 F4P6 pin-mux：TIM1 default `PD2/PA1/PC3/PC4`→PWM1–4；TIM2 remap 3 `PC1/PC7/PD6/PD5`→PWM5–8；I²C remap `PC5=SCL/PC6=SDA`；`PD1=SWIO`、`PD7=NRST`。Operator 报价：CH32V003F4P6 ¥1.7718、74HC244PW,118 ¥1.6281，合计 ¥3.3999/板；两者均为扩展库。两块贴装报价中的换料/扩展库费用仍待最终 PCBA quote 核对。

### 第一批暂不需要代查

- 电阻、电容和普通 0 Ω/测试点：待电路表完整后统一归并，避免现在逐项查料。
- TCA9539、ICM-42670-L、HAL49ESO、PDM microphone、WS2812C：exact function/part 已由 Design Input 给出，当前没有要求廉价替代。PCA9685 已由 v0.4 明确替换为 `CH32V003F4P6 + 74HC244PW,118`。
- 扬声器 `ZX-MX1.25-2PZZ / C7430458`：Operator 已提供并通过原厂图纸审阅，CDB-002 已关闭，无需继续代查。

## 3. 第二批：主功率器件

Design Input Freeze v0.4 规范性保留 v0.3 的 power-system 输入。按 topology-first 顺序分批查询，避免一次搜索全部功率器件。当前只激活 PWQ-01、PWQ-03、PWQ-04；不要求现在查询 PWQ-05..12。

| Query ID | 位置                                  | 后续候选类别                                                        | 等待的输入                                                  |
| -------- | ----------------------------------- | ------------------------------------------------------------- | ------------------------------------------------------ |
| PWQ-01   | 电池入口不可绕过保护                          | physical fuse 与 holder/footprint                              | v0.3 §2/§3.1 包络已冻结；等待候选与后续协调计算 |
| PWQ-02   | 3S reverse/UV/OV/current protection | external-FET controller，或资料完整的等价组合                            | v0.3 §2/§3；`LTC4368IMS-2#PBF` 当前只是纸面候选，不是采购锁定 |
| PWQ-03   | battery protection FET pair         | 30 V 级 back-to-back N-MOSFET                                  | v0.3 §2/§3 与后续 SOA/thermal 计算 |
| PWQ-04   | UBEC input branch protection        | eFuse/controller + external FET，按 v0.3 支路包络选型 | 不得依赖 YSIDO 未知内部保护 |
| PWQ-05   | USB POWER Type-C current detection  | Default/1.5 A/3 A sink detector/controller                    | v0.3 §5 状态已冻结 |
| PWQ-06   | USB POWER OVP/ILIM/reverse block    | 5 V、最高 3 A input protection                                   | v0.3 §2/§4/§5 |
| PWQ-07   | UBEC/USB servo source isolation     | ideal-diode/back-to-back-FET controller 或合格 integrated switch | v0.3 §2/§4/§5 |
| PWQ-08   | battery/USB logic source selection  | reverse-blocking priority mux/ideal-diode architecture        | v0.3 §2/§4/§5 |
| PWQ-09   | 3V3 regulator                       | 5 V 与 9–12.6 V 输入均适用、3.40 V nominal、≥2.5 A 的 synchronous buck | v0.3 §2/§5 |
| PWQ-10   | servo current measurement           | shunt + current-sense amplifier 或有充分理由的替代                     | v0.3 §2 |
| PWQ-11   | transient clamp / negative clamp    | servo rail 与 battery rail TVS/clamp                           | v0.3 §2/§4 |
| PWQ-12   | power-area temperature monitor      | digital sensor 或 NTC frontend                                 | v0.3 §2 与剩余 TCA/ADC 资源确认 |

### 3.1 当前查询：PWQ-01 板载实体保险丝

Circuit Design 方向：选择一只板载焊接、不可复位的 fast-acting fuse；SMD 或 THT 均可，不增加可绕过的跳线，也不要求占板较大的可更换保险座。它是 catastrophic backup，active protection 仍单独存在。

请在嘉立创返回 2–4 个候选：

- 类别必须是一次性 chip fuse / fast-acting fuse，排除 PPTC、自恢复保险丝和只有 AC rating 的器件；
- 优先 `10 A`，可附带价格明显更好的 `8 A` 或 `12 A` 同系列候选，最终额定值之后按 time-current/`I²t` 选择；
- manufacturer guaranteed rated voltage `≥32 VDC`；
- manufacturer guaranteed DC breaking/interruption capacity `≥100 A`；若页面没有这项，不要自行推断，在结果中写“datasheet 待核对”；
- 1206、2410、2920 等贴片封装或小型 radial/axial THT 均可；Basic/Extended 都可，记录是否扩展库；
- 当前已知参考族为 Bel C1H / `0685H9100-01`，可以返回它的嘉立创编号及更便宜的同等级候选。

第一轮只需要 `LCSC code + exact MPN + manufacturer + package + 单价/库存 + 页面链接`。我筛到 1–2 个 finalists 后，你再下载原厂 PDF。

### 3.2 当前查询：PWQ-03 LTC4368 背靠背 N-MOSFET

Circuit Design 方向：两颗相同的单 N-channel MOSFET 背靠背，不采用难找的双管专用封装。

请返回 3–5 个候选：

- N-channel enhancement MOSFET；`VDS ≥30 V`，同等成本下优先 40 V；
- datasheet **maximum** `RDS(on) ≤10 mΩ @ VGS=10 V` 优先；`≤15 mΩ` 是现实可接受范围；`≤20 mΩ` 只有在 TO-252/DFN5×6 等大封装且热数据充分时才保留；不要只报 typical；
- 优先 DFN 5×6、PowerPAK SO-8、LFPAK56、TDSON-8 等带大散热焊盘封装；DPAK 可作为较大备选；
- manufacturer datasheet 必须有 DC/pulsed SOA 图和 transient thermal impedance；第一轮页面无法确认时明确标注，不要猜；
- 不按商城“连续 100 A”等标题筛选，关键是 exact `RDS(on)`、SOA、package 和可装配性；
- 每板使用两颗，因此单价请按两颗考虑。

第一轮只需要候选页面信息；我 shortlist 后再请你提供相应原厂 PDF。

### 3.3 当前查询：PWQ-04 UBEC 输入支路 eFuse

Circuit Design 方向：由于目前集成 eFuse 的 6 A 以上器件价格过高，优先查询“热插拔/电子保险丝保护控制器 + 外置 N-MOSFET”；仅在价格和热能力都合理时才回到单颗集成 eFuse，不先堆叠两个独立 5 A eFuse。

嘉立创搜索关键词可用：`热插拔控制器`、`电子保险丝控制器`、`过压过流保护控制器`、`hot-swap controller external MOSFET`。不要把只有 ideal-diode、只有 gate driver、或没有 current-limit/circuit-breaker 的器件当成 eFuse 候选。

请返回 2–4 个候选：

- guaranteed operating input range 必须完整覆盖 `9.0–12.6 V`，absolute maximum 至少 18 V，优先 24 V；
- 必须有 external N-MOSFET gate-drive/output，且外置 FET 的电流不受控制器内部 5 A pass switch 限制；
- 必须有可设定的 over-current limit 或 circuit-breaker，目标可覆盖 `7.5–10 A`；不要只选 5 A 集成 pass-FET 等级；
- 必须有 hardware enable，且外部下拉能够实现 default-off；
- 优先具有 soft-start/dVdt、UV/OV、thermal fault 和 fault flag；
- reverse-current blocking 或 disabled-state reverse blocking 若有请明确记录；没有也可以返回，由我结合外置背靠背 FET 判断 topology；
- 可作为高压/高成本参考的家族包括 `TPS2490/TPS2491`、`LM5069`、`LTC4218` 等；请优先寻找嘉立创上更便宜、仍有原厂资料的同类器件，不要直接假定这些参考型号最终采用。

第一轮只返回页面信息，不需要替我判断最终合格与否；原厂 PDF 等 shortlist 后再提供。

### 3.4 Operator 第一轮返回与初筛（2026-08-30）

| Query | Candidate | 初筛状态 | 原因 / 下一步 |
| --- | --- | --- | --- |
| PWQ-01 | JKSEMI `12 100.10 / C355692`, 1206, 10 A, Extended, ¥0.275@10+ | **MPN SELECTED / ANALYSIS DEFERRED** | Operator PDF 确认 `12 100 Series` 是 Fast Acting SMD Fuse；10 A row 为 32 VDC/150 A DC breaking（同时列 24 VDC/300 A）、typical cold resistance 6.8 mΩ、typical drop 100 mV、typical pre-arcing `I²t=20 A²s`；6–40 A 规格在 1.0×In 保持 ≥4 h、3.5×In ≤5 s。满足 v0.3 的 voltage/interruption minimum；temperature derating、8 A waveform nuisance margin 和 active-protection coordination 按指示后算 |
| PWQ-01 | `2009T10A250V`, THT, ¥0.358245 | **NOT IDENTIFIABLE YET** | 缺 LCSC code 和 manufacturer；`250 V` 页面标题不能证明 DC breaking capacity；型号中的 `T` 常用于 time-lag 标识但不得仅凭名称下结论。若希望保留，请补 LCSC code、manufacturer 和 PDF |
| PWQ-01 | `T10A 250V / C54524754`, THT, ¥0.22131 | **LOW PRIORITY / DATA NEEDED** | 插件形式可以，但 `T10A` 很可能是 time-lag 类而不是冻结要求的 fast-acting；且 250 VAC 不能替代 ≥32 VDC/≥100 A DC 证明。只有原厂 PDF 同时证明 fast/DC interruption 才保留 |
| PWQ-03 | Bestirpower `BCD65N320Y1 / C53426991`, DPAK, ¥1.7565 | **REJECTED BY MANUFACTURER DATA** | Operator PDF Rev. 0.5 确认 SiC、650 V、10 A、`RDS(on)=320 mΩ typ / 440 mΩ max @15 V, 4 A`；适合高压开关而不是 3S 低损耗 pass FET。即使耐压和电流标题很高，串入本项目会产生不可接受的导通损耗 |
| PWQ-03 | `IPD60R360P7S-HXY / C52098372`, ¥2.603 | **REJECTED / PDF NOT NEEDED** | exact MPN 命名指向 600 V / 360 mΩ class，与已由原厂资料确认淘汰的两颗 SiC 器件同属错误搜索方向；无需继续投入资料查询 |
| PWQ-03 | MOT `MOT75CL300D / C54102073`, TO-252, ¥3.08 | **REJECTED BY MANUFACTURER DATA** | Operator PDF Rev. A.1.1 确认 SiC、750 V、10 A、`RDS(on)=300 mΩ typ / 400 mΩ max @15 V, 4 A`；同样是高压开关器件，不适合 3S 低损耗 pass path |
| PWQ-03 | `HD506N032SG / C52766425`, ¥3.95 | **COST-SECONDARY / DATA NEEDED** | 页面给出 2.6 mΩ@10 V；每板两颗成本约 ¥7.90，性能余量大但不符合“尽可能低成本”优先级。请仅在更便宜候选资料不成立时提供 PDF，并核对 maximum RDS(on)、VDS、package/SOA |
| PWQ-03 | Hongjiacheng `HD303N080SG / C52766420`, PDFN3030, 30 V, ¥1.1421 | **MPN SELECTED / ANALYSIS DEFERRED** | Operator PDF Rev. 1.0 确认 PDFN3030、VDS=30 V、`RDS(on)<8 mΩ @10 V, 12 A`（4.5 V 时 <11 mΩ）、ID=28 A、RθJC=6 °C/W、SOA/瞬态热阻图；每板两颗约 ¥2.28。它是目前最低成本且具备合理热封装的 pass-FET 候选；30 V clamp/AV margin、最终 SOA 和热计算按指示暂缓 |
| PWQ-03 | Hongjiacheng `HT304N200S / C53055313`, SOT-23-3L, 40 V, ¥0.5214 | **REJECTED BY MANUFACTURER DATA** | Operator PDF Rev. 1.0 确认 SOT-23-3L、`RDS(on)<20 mΩ @10 V, 5 A`、ID=7 A、PD=1.1 W、RθJA=113 °C/W；两颗背靠背用于 6 A path 时封装热余量不足，低单价不能弥补散热风险 |
| PWQ-04 | `TPS259470LRPWR / C3662793`, ¥6.23 | **PROVISIONAL REJECT / CURRENT LIMIT TO CONFIRM** | 该 exact family 预期是约 5.5 A class，低于 6 A continuous，但没有本轮原厂正文时不作为 confirmed fact。请只看页面/PDF 的 maximum programmable/current-limit range；若上限 `≤5.5 A` 即淘汰，无需下载保存 |
| PWQ-04 | `TPS259474ARPWR / C3662807`, ¥7.34 | **PROVISIONAL REJECT / CURRENT LIMIT TO CONFIRM** | 与上项同族且更贵；若原厂资料确认上限 `≤5.5 A`，淘汰。只有 guaranteed operating/current-limit range 覆盖 6 A continuous 且允许 7.5 A transient 才继续 |
| PWQ-04 | `MX25924D33 / C5875699`, ¥? | **REJECTED FOR UBEC 6 A BRANCH** | Operator PDF v1.1（Wuxi Maxin Micro）确认 VIN operating 4.5–13.8 V、recommended `IOUT=0–4 A`、absolute `IOUT=5 A`、ILMT max 约 5.6 A、内部 FET `RDS(on)=20 typ/48 mΩ max`；不满足 6 A continuous。资料没有 parallel-operation/均流应用说明，因此 2×并联也不能按 10 A 处理；其 BFET pin 只能驱动额外 blocking FET，不能绕过内部 28 mΩ pass FET |
| PWQ-04 | `WSD7040AF / C48996608`, DFN-14L (5×6), 4.5–28 V, max continuous 6 A, 46 mΩ | **PROVISIONAL REJECT FOR UBEC / POSSIBLE USB CANDIDATE** | 页面给出的最大连续电流正好等于 6 A target，没有 design margin；按页面 RDS(on) 粗估 6 A 时约 1.66 W、8 A 瞬态约 2.94 W，明显压缩 v0.3 的 40°C/30°C thermal budget。还未确认 current-limit、reverse blocking、SOA 和 thermal derating。若原厂 PDF 无特殊并联/散热保证，不用于 UBEC 主支路；可留作 ≤3 A USB/低电流路径候选 |
| PWQ-04 | `NCP45524IMNTWG-L / C892472`, 0.5–13.5 V, 18 mΩ, max continuous 6 A | **CONDITIONAL CANDIDATE / NOT YET ACCEPTED** | 比 WSD7040AF 的导通电阻低；页面粗估 6 A 时约 0.65 W、8 A 瞬态约 1.15 W。但最大连续仍仅 6 A，且 13.5 V input ceiling 距 3S 满电 12.6 V 仅 0.9 V，必须确认 transient/absolute rating、recommended continuous current、current-limit upper bound、reverse blocking、thermal derating 和 default-off EN。若这些不能覆盖 v0.3 envelope，则转 USB/低电流用途或淘汰 |
| PWQ-04 | `TPS259823LNRGER`, user candidate, ¥18.54 | **REJECTED FOR UBEC INPUT / POSSIBLE 5 V USB CANDIDATE** | TI TPS25982 Rev. D datasheet covers base `TPS259823LNRGE` and specifies `VOVP(R)=7.6 V` (7.39–7.76 V), with test condition for the 823x variant at VIN=5 V. A 9–12.6 V UBEC input would trip overvoltage; the family’s 15 A/2.7 mΩ headline does not change this. For 3S input search `TPS259824` (16.9 V OVP variant) or `TPS259827` (no OVLO), then confirm exact suffix/package and protection behavior. The 823x may still suit a 5 V USB branch |
| PWQ-04 | `TPS259827LNRGER`, user candidate, ¥21.80,现货 | **TECHNICALLY ACCEPTABLE INTEGRATED UBEC CANDIDATE / COST SECONDARY / ANALYSIS DEFERRED** | TI TPS25982 Rev. D order table covers base `TPS259827LNRGE`: 2.7–24 V family, no OVLO variant, active current limiter (`L`), QFN/RGE package. Unlike 823, it does not trip at 3S 9–12.6 V. `RILIM` is programmable (datasheet example 182 Ω gives typ. 8.13 A, 7.23–9.07 A across temperature); RETRY_DLY tied low provides latch-off. This is a much better single-chip fit than two unsupported 5 A parts. Formal ILIM tolerance, thermal, SOA, branch drop and cost comparison are intentionally deferred. JLC exact item is reported in stock at ¥21.80; base/reel suffix correspondence remains to be checked on the item page. No-OVLO means upstream LTC4368 battery OV/UV and fuse remain mandatory. |
| PWQ-04 | Winsemi `WS7025AD / C48996615`, DFN5×6-16L, VCC 4.5–28 V, RON 30 mΩ typ/60 mΩ max, IL(NOM)=7 A@25°C、5.5 A@85°C | **REJECTED FOR UBEC / POSSIBLE USB CANDIDATE** | Operator PDF A0.3 确认它是双通道 automotive high-side driver；85°C nominal load 仅 5.5 A，不能作为 v0.3 的 6 A continuous 主支路；current limit `ILIMH` typ 24 A（12–36 A range）也不能承担 7.5–10 A branch protection。RON 在 6 A 下约 1.08 W typ、在高温 max 条件更高。可保留作 ≤3 A USB/低电流高边开关，其 current sense/diagnostics 有价值 |

本轮 manufacturer evidence 记录：

- manufacturer=`JKSEMI（金开盛）`；exact MPN=`12 100.10`；document title=`1206 Fast Acting SMD Fuses — 12 100 Series`；document/approval no.=`E355868`；revision=`版次 c`；date=`2021-10-21`；source item=`LCSC C355692`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`66A49940E0F3B35D3EF848CEFDD4BDE0(1).pdf`；SHA-256=`ae16fb34d0ce1804321cf92fed8d4b5cc570bc20aba4fc7d0e0a504869c56bcb`。
- manufacturer=`MOT`；exact MPN=`MOT75CL300D`；document title=`MOT75CL300D POWER SIC MOSFET`；revision=`A.1.1`；date=`2025/12`；source item=`LCSC C54102073`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`C54102073_碳化硅场效应管(MOSFET)_MOT75CL300D_规格书_MOT75CL300D-VA.1.1.PDF`；SHA-256=`11ba13eb9d7199133e7f52a666662c5186cafe5f867f7fdd69af06040f080784`。
- manufacturer=`Bestirpower`；exact MPN=`BCD65N320Y1`；document title=`BCD65N320Y1 N-Channel Silicon Carbide Power MOSFET`；revision=`0.5`；publication date=`not stated`；source item=`LCSC C53426991`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`C53426991_碳化硅场效应管(MOSFET)_BCD65N320Y1_规格书_BCD65N320Y1.PDF`；SHA-256=`a9cd6b8a8302f77f41c20ffb23abe460b19083695a4164654f411a6ada3807fc`。
- manufacturer=`Zhuhai Hongjiacheng Technology`；exact MPN=`HD303N080SG`；document title=`HD303N080SG N-CHANNEL MOSFET`；revision=`1.0`；date=`2025-11-04`；source item=`LCSC C52766420`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`C52766420_场效应管(MOSFET)_HD303N080SG_规格书_HD303N080SG_REV1.0.PDF`；SHA-256=`25584cdf9744734bc82df7806f05a67bb7122ed63b7c9ba14b1173661dfc786a`。
- manufacturer=`Zhuhai Hongjiacheng Technology`；exact MPN=`HT304N200S`；document title=`HT304N200S N-CHANNEL MOSFET`；revision=`1.0`；date=`2025-11-21`；source item=`LCSC C53055313`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`C53055313_场效应管(MOSFET)_HT304N200S_规格书_HT304N200S_REV1.0.PDF`；SHA-256=`930a1c2940966f11e347afa0502b17bab89a438bcf5585e4fcebabe53d6b2fc7`。
- manufacturer=`Wuxi Maxin Micro`；exact MPN=`MX25924D33`；document title=`MX25924 12V electronic fuse with overvoltage clamp and blocking FET control`；revision=`V1.1`；date=`not stated`（PDF metadata 2023-04-21）；source item=`LCSC C5875699`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`89F6099C62BC1A7ECF6FBE71DE7EC8FF.pdf`；SHA-256=`b6d6e51e942deab9e4910f82c87a2c047c31c89ec897653cee10004dc630973f`。
- manufacturer=`Winsemi Microelectronics`；exact MPN=`WS7025AD`；document title=`WS7025AD Product Description — High-side driver with current sense analog feedback for automotive applications`；revision=`A0.3`；date=`not stated`（PDF metadata 2024-08-30）；source item=`LCSC C48996615`（Operator 未提供 URL）；access date=`2026-08-30`；local review copy=`C48996615_功率电子开关_WS7025AD_规格书_WS7025AD+DATASHEET_A0.3_EN(1).PDF`；SHA-256=`6e0b08703819b246d327c8d8eadd0a7e29f08483df98bd38f249601ed18c7637`。
- manufacturer=`Texas Instruments`；exact family/orderable base=`TPS259823LNRGE`（JLC suffix `LNRGER` 的 reel suffix 需按 item 确认）；document title=`TPS25982 2.7V 至 24V、2.7mΩ、15A 智能电子保险丝`；document no.=`SLVSEI3`；revision=`D`；date=`2026-07-03`（PDF metadata；正文 revision/date 以 datasheet 为准）；source item=`LCSC candidate TPS259823LNRGER`；access date=`2026-08-30`；local review copy=`tps25982.pdf`；SHA-256=`b6d6e51e942deab9e4910f82c87a2c047c31c89ec897653cee10004dc630973f`。
- manufacturer=`Texas Instruments`；exact family/orderable base=`TPS259827LNRGE`（JLC candidate `TPS259827LNRGER` 的 reel suffix 和库存价格需按 item 确认）；document title=`TPS25982 2.7V 至 24V、2.7mΩ、15A 智能电子保险丝`；document no.=`SLVSEI3`；revision=`D`；date=`2026-07-03`（PDF metadata；正文 revision/date 以 datasheet 为准）；source item=`LCSC candidate TPS259827LNRGER`；access date=`2026-08-30`；local review copy=`tps25982(1).pdf`；SHA-256=`b6d6e51e942deab9e4910f82c87a2c047c31c89ec897653cee10004dc630973f`。
- manufacturer=`Wuxi Maxin Micro`；exact MPN=`MX74610SS`；document title=`MX74610 Reverse Polarity Protection Smart Diode Controller`；revision=`V12`；date=`not stated`（PDF metadata 2023-11-21）；source item=`LCSC C19269866`；access date=`2026-08-30`；local review copy=`C19269866_理想二极管-ORING控制器_MX74610SS_规格书_WJ728460.PDF`；key data confirmed: external N-MOS reverse-polarity/ideal-diode controller, 0–55 V recommended cathode-anode range, 65 V/2 ms absolute, 2 µs reverse response, MSOP8, operating −40…125 °C。
- manufacturer=`Wuxi Maxin Micro`；exact MPN=`LMX5069MS`；document title=`LMX5069 Highside Hot Swap Controller With Power Limiting Function`；revision=`V15`；date=`not stated`（PDF metadata 2026-04-24）；source item=`LCSC C47967145`；access date=`2026-08-30`；local review copy=`C47967145_浪涌保护器_LMX5069MS_规格书_WJ1881538.PDF`；key data confirmed: external N-MOS high-side hot-swap/current-limit/power-limit controller, 9–90 V operating range, programmable UVLO/OVLO/current/power/timers, MSOP10, RθJA 156 °C/W；fault mode includes automatic restart, so deterministic UBEC short-fault recovery still needs circuit decision。
- manufacturer=`JieJie Microelectronics`；exact MPN=`JMSL0302AU-13`；document title=`JMSL0302AU 30V 1.2mΩ N-Ch Power MOSFET`；revision=`2.7`；date=`not stated`（PDF metadata 2021-06-24）；source item=`LCSC C2890409`；access date=`2026-08-30`；local review copy=`C2890409_场效应管(MOSFET)_JMSL0302AU_规格书_WJ272936.PDF`；key data confirmed: PDFN3x3 8-pin, VDS 30 V, RDS(on) typ 1.2 mΩ@10 V / 2.0 mΩ@4.5 V, TJ −55…150 °C, RθJA max 65 °C/W, RθJC max 2.5 °C/W；**MPN selected for both QREV and QPASS as two separate physical MOSFETs**，QPASS 的 linear-mode SOA 仍须按 LMX5069 startup/current-limit waveform 验证。 |

`KP40512SGA` / user candidate（0.3–12 V、12 A、9 mΩ）— **UBEC 输入预淘汰；6 V 舵机侧条件候选**：若 `12 V` 是 guaranteed operating maximum，则不能承受 3S 满电 `12.6 V`，更没有 battery transient margin；不能用于 `VBAT_SWITCHED`→UBEC 输入。9 mΩ/12 A 对 6 V servo-side path 有吸引力，但必须提供 exact manufacturer PDF，确认 VCC absolute/operating range、current-limit/short behavior、reverse blocking、EN default-off 和 thermal rating。

`FR015L3EZ / C891259` / user candidate — **TOPOLOGY REJECTED**：低侧反向偏置/反极性保护不能替代本项目电池或 UBEC 正电源路径的高侧隔离。低侧切断会抬高或切换系统 GND，破坏 USB、ESP32、舵机和 ESTOP 的共同参考，并可能形成不可预期的外部回流；除非未来另有明确的低侧负载接口需求，否则不要求继续查其 exact 参数或 PDF。

下一轮不再需要查询三颗 SiC MOSFET；当前优先请提供 `HT304N200S / C53055313` 的 manufacturer、package 和 PDF；若其 maximum RDS/SOA 不合格，再提供 `HD303N080SG / C52766420` PDF。`HD506N032SG / C52766425` 仅作为高性能高成本备用。普通低压 silicon N-MOSFET 搜索范围保持 30–40 V、maximum `RDS(on)≤10–15 mΩ @10 V`，必要时大封装可放宽至 `≤20 mΩ`；`≤5 mΩ` 只是优选，不是硬性 Design Input。eFuse 方面请提供 `MX25924D33 / C5875699` 原厂 PDF；若正文不明确支持并联，则放弃双芯片方案。由于单颗 ≥7 A integrated eFuse 价格过高，Circuit Design 同时把“external-FET protection controller”恢复为成本优先候选，不要求继续硬找昂贵集成件。

### 3.5 两块首板的阶段性物料成本审计（2026-08-30）

本节只统计 Operator 已提供单价且当前仍为 selected/candidate 的板载器件，不是嘉立创 PCBA 报价。未包含 PCB、钢网/贴装、扩展库换料费、运费、税费、损耗数量、通用阻容，以及尚未选型或缺价的器件。

| 分组 | 当前计入器件 | 每板小计 | 两板小计 |
| --- | --- | ---: | ---: |
| 已选且已有价格的核心功能（不含新 PWM engine） | ESP32-S3 N16R8 ¥31.60；ICM-42670-L ¥7.00；PDM mic ¥2.40；MAX98357A ¥3.01；HAL49ESO 仅装 1 颗 ¥0.7052；4×WS2812C ¥2.4296；4×H5VL10B ¥0.16 | ¥47.3048 | ¥94.6096 |
| 新 PWM engine | `CH32V003F4P6 / C5187096` ¥1.7718 + `74HC244PW,118 / C5623` ¥1.6281；两者均为扩展库；已取代每板 ¥20.39 的 PCA9685 | ¥3.3999 | ¥6.7998 |
| 低成本 battery active protection 候选 | fuse ¥0.275；MX74610SS ¥2.99；LMX5069MS ¥5.97；2×JMSL0302AU-13 ¥3.36 | ¥12.5950 | ¥25.1900 |
| **已知价格阶段小计** | 上述已有价格项目 | **¥63.2997** | **¥126.5994** |
| 尚未锁定但已有历史报价的候选 | TUSB320 ¥4.83；INA181A2 ¥2.0707 | +¥6.9007 | +¥13.8014 |
| **含两项 pending candidate** | 不代表最终选择 | **¥70.2004** | **¥140.4008** |

PCA9685 的旧成本为 ¥20.39/板、¥40.78/两板，现已由 v0.4 删除。新 PWM engine 为 ¥3.3999/板、¥6.7998/两板，因此元件单价净节省 **¥16.9901/板、¥33.9802/两板**。CH32 与 74HC244 均为扩展库，即以两个扩展库料号取代原一个料号；最终 PCBA 净节省必须减去新增的一个 unique extended-part 换料/贴装费用，当前没有 quote 时不把 ¥33.9802 宣称为整单节省。当前剩余最大单项是 ESP32-S3 N16R8 ¥31.60/板；其次是 LMX5069MS、ICM-42670-L、TUSB320 类器件。MAX98357A、ESD、小信号晶体管和普通阻容继续压价的收益有限。

尚未计价：TCA9539、reset supervisor、两个 Type-C、USB ESD/NPN、XT30、两只 UBEC 板端端子、MASTER switch、speaker connector、3V3 regulator/inductor、USB protection、logic source mux、UBEC/USB servo isolation、servo shunt/current-sense 最终件、TVS/clamps、ESTOP hardware latch、bulk capacitors 和全部通用阻容。只有取得这些 exact MPN 的单价/库类型，以及 PCB 尺寸层数和贴装面信息后，才能形成两块 PCBA 的价格上界。

## 4. 返回格式示例

```text
LVQ-03
- LCSC code:
- exact MPN:
- manufacturer:
- package:
- basic/extended:
- stock / unit price:
- JLC/LCSC URL:
- manufacturer datasheet URL:
- notes or uncertainties:
```

未找到合格候选时直接写“未找到”，不要用相近商品参数替代关键资料。设计者会据此保留 `PENDING`、调整 topology，或登记新的 `COMPONENT_DATA_BLOCKER`。
