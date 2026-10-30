# 牛来快跑 PCB Controller Rev A — Reference Design 与模块可移植性审阅 v0.1

状态：**Draft / Reference review；不是 Design Input、Circuit Design Freeze 或 schematic authorization**

日期：2026-08-31

Owner：Project maintainer

权威设计输入：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md`](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md)

活跃问题台账：[`PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md`](../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md)

## 1. 目的、约束与结论等级

本文补上此前缺失的成熟 Reference Design 和同功能模块审阅，目标是在**不牺牲已冻结功能**的前提下减少自创电路、首板返工和总体成本。以下边界固定：

- 保留 `ESP32-S3-WROOM-1-N16R8`；N4 与 N16R8 的小额差价不足以支持降低 Flash/PSRAM 余量。
- 保留板载 `SY8388ARHC` 6 V buck、USB POWER、USB DATA、8 路舵机、CH32 PWM、硬件 ESTOP、IMU、Hall、麦克风、功放、LED、足端输入、屏幕接口和已冻结扩展能力。
- 不恢复 v0.6 已删除的外置 UBEC fallback/interface。
- Reference Design 只能证明某个连接、布局或系统模式已被成熟实现；不能自动证明本项目的电压、电流、温升、故障或成本要求。
- 商业模块的宣传额定值、图片和用户经验不等于器件 guaranteed data；不能用来关闭 DIB、CDB 或 PVR。
- 本文不授权 schematic、netlist、ERC、layout、采购或实体测试。

本文使用四级结论：

| 等级 | 含义 |
| --- | --- |
| `DIRECT` | exact device/module 的官方参考，可直接作为连接或 footprint/layout 输入，但仍需按本项目条件复算 |
| `PORTABLE` | 同功能成熟模块/项目中有可迁移的拓扑、接口、启动或故障处理原则 |
| `CONDITIONAL` | 只在电压、封装、负载或功能差异关闭后可迁移 |
| `NON-EVIDENCE` | 可帮助提出问题，但不能作为本板合格证据或关闭 blocker |

## 2. 已下载的公开仓库快照

以下仓库于 2026-08-31 以 `--depth 1` 下载到临时目录 `/tmp/niulai-pcb-reference.dPIGGg/`；临时目录不进入仓库或 MR：

| 仓库 | 审阅 commit | 用途 | License/使用边界 |
| --- | --- | --- | --- |
| [Espressif KiCad Libraries](https://github.com/espressif/kicad-libraries) | `dd76561812ab300351234ba6e0ec1295641796f0` | 官方 `ESP32-S3-WROOM-1` symbol、footprint、STEP 模型和 DevKit footprint | Apache-2.0；后续 CAD 应固定所用 library release/commit |
| [Espressif ESP DevKits](https://github.com/espressif/esp-dev-kits) | `65894025e6697b2fc04d6b246efb2628a8d84687` | DevKitC-1 系统框图、pin layout、USB/供电使用方式和文档入口 | Apache-2.0；开发板整体不是本项目电源树模板 |
| [Petoi OpenCat ESP32](https://github.com/PetoiCamp/OpenCatEsp32-Quadruped-Robot) | `9ebb48ac7c651a0237fa60c4a9819bca542f5036` | 成熟四足控制器的 12/16 路 servo、IMU、IR、NeoPixel、电池监测、扩展模块与故障经验 | GPL-3.0；本文只提取设计经验，复制代码前必须单独做许可证评估 |

这些 commit 是本次观察的可追溯输入，不意味着团队接受其后续更新或其全部设计选择。

本轮还逐项审阅以下原厂入口；它们不是仓库镜像，后续设计时应保存所用PDF revision与SHA-256：

- [Espressif ESP32-S3 Hardware Design Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/)：模组摆位、天线keep-out、USB和电源/strap检查；`latest`会变化，进入CAD前必须固定revision。
- [TI TUSB320EVM](https://www.ti.com/tool/TUSB320EVM)：Type-C UFP/DFP/DRP功能评估入口；本项目只迁移sink/UFP相关部分。
- [TI TPS2121EVM-023](https://www.ti.com/tool/TPS2121EVM-023)：priority power mux的schematic、layout、BOM和测试配置入口。
- [TI LM74800EVM-CD](https://www.ti.com/tool/LM74800EVM-CD)：back-to-back NFET、reverse battery/current与OV path评估入口。
- [TI INA180-181EVM](https://www.ti.com/tool/INA180-181EVM)：只作gain、REF与功能评估；TI明确声明其fixture不是目标layout/EMC模板。
- Silergy `SY8388ARHC` Rev.1.0 datasheet、AN与官方产品页/EVB下载入口：本轮确认入口存在，但未成功取得完整`EVB_SY8388ARHC` design package，因此不伪造一个可用本地快照或关闭该过程缺口。

## 3. 总体映射

| 牛来快跑功能块 | Reference Design / 同功能模块 | 等级 | 当前处理 |
| --- | --- | --- | --- |
| ESP32-S3 模组、BOOT/RESET、USB、天线 | Espressif DevKitC-1、S3 Hardware Design Guidelines、官方 KiCad library | `DIRECT` | 复用官方 footprint/keep-out/USB/EN/BOOT规则；保留 N16R8 |
| 多舵机系统与外设并存 | Petoi BiBoard/OpenCat ESP32 | `PORTABLE` | 迁移校准、关断、低电压和模块初始化经验；不照搬直出 PWM 电气架构 |
| 8 路 PWM engine | OpenCat direct PWM、PCA9685 两条成熟路径；Adafruit PCA9685 模块 | `PORTABLE` | 保留已选 CH32+74HC244；用成熟项目暴露的故障补强验收 |
| 6 V/6 A、8 A 瞬态 buck | Silergy `SY8388ARHC` datasheet、AN、`EVB_SY8388ARHC` | `DIRECT` | 以 exact EVB/AN 为功率环路和热布局起点；不能用额定 8 A 替代本板热证据 |
| USB Type-C current advertisement | TI `TUSB320EVM` | `DIRECT`/`CONDITIONAL` | 迁移 UFP GPIO mode、CC 和 AON 供电原则；删除 EVM 的 DFP/DRP/jumper 功能 |
| battery/USB logic source mux | TI `TPS2121EVM-023` | `DIRECT`/`CONDITIONAL` | 迁移 priority、OV、SS、ILM、测试点和 HotRod layout；按本板 reset 策略重算 |
| battery reverse/OV protection | TI `LM74800EVM-CS/CD` 与 application brief | `CONDITIONAL` | 只迁移 B2B MOSFET/ideal-diode 拓扑和瞬态检查；整板数值由v0.7逐板EOL关闭证据缺口 |
| servo current sense | TI `INA180-181EVM` + datasheet layout | `CONDITIONAL` | 功能/增益评估可迁移；EVM 官方明确不是目标 EMC/layout 模板 |
| I2S 功放 | Adafruit MAX98357 breakout PCB | `PORTABLE` | 迁移最小 I2S/BTL/SD/GAIN/去耦拓扑；3.4 V WLP 和声学/EMI需本板验证 |
| ICM-42670-L | TDK exact datasheet/application circuit；OpenCat ICM42670 driver | `DIRECT` + `PORTABLE` | 电气只信 exact L 数据；迁移自检、校准、初始化顺序与 raw-data logging 思路 |
| WS2812、IR、显示和模块扩展 | OpenCat/Adafruit 同类模块实现 | `PORTABLE`/`CONDITIONAL` | 迁移驱动隔离、可禁用和模块化测试；exact rail/connector仍按当前 EDD/PVR |

## 4. ESP32-S3：直接复用官方实现

### 4.1 可直接迁移

1. 使用官方 `ESP32-S3-WROOM-1` symbol、footprint 和 STEP，不自行重画 land pattern。后续原理图/PCB 必须把 library commit 或发布版本写入生成记录。
2. 模组天线尽量伸出主板边；若不能伸出，遵循官方 base-board keep-out。官方建议模组天线周围至少 15 mm clearance，并在成品外壳条件下验证吞吐量和通信距离。
3. USB D-/D+ 使用 GPIO19/GPIO20，靠 ESP 预留 22/33 Ω 串阻和 DNP 对地电容；90 Ω ±10% 差分、连续参考地、少过孔。当前 EDD-011 已基本符合。
4. 保留 BOOT、RESET 和 UART0 recovery pads。原生 USB 可完成下载与 Serial/JTAG，但 UART recovery 仍是 USB layout/firmware 失败后的廉价回滚路径。
5. 继续使用 N16R8，并保留 GPIO35–37 的 Octal PSRAM reservation；不得为了多三个 GPIO错误占用这些脚。

### 4.2 不能照搬

- DevKitC 的开发板 LDO、USB-to-UART bridge、双 USB 供电和排针不是本项目 `3.40 V / 2.5 A continuous / source mux` 的参考电源树。
- DevKitC 的宽松机械空间不包含本项目的 SY8388、舵机母线、Class-D、麦克风和 IMU 共板耦合问题。
- 官方 footprint 解决封装正确性，不替代 JLC assembly layer、paste aperture、antenna-to-enclosure 或 DFM review。

### 4.3 对当前 EDD 的影响

- EDD-011、013、014 的方向保持，不需要降配或更换 N16R8。
- 原理图前新增一项机械输入检查：75 × 60 mm 板框中，官方天线 keep-out、四角安装孔、USB 插头包络和高电流区必须能同时放下；否则先调整摆位，不通过缩小 keep-out解决。

## 5. 成熟四足控制器：OpenCat/BiBoard 的可移植性

OpenCat ESP32 当前公开仓库描述 BiBoard 可驱动最多 12 路舵机，并同时使用 ICM42670、IR、NeoPixel、电池检测和扩展模块。它证明“ESP32 + 多舵机 + 多传感/反馈功能共存”是成熟产品路径，但公开软件仓库不包含足以验证本项目 3S/6 A/USB 双源电源树的板级资料。

### 5.1 PWM 架构的关键负面经验

OpenCat 同时保留两类 servo backend：

- 早期 BiBoard 用 ESP32 PWM 直接驱动 12 路舵机；
- BiBoard2 可切换到 PCA9685 I2C PWM driver。

其 `src/espServo.h` 明确记录：共享 timer 生成多路 PWM 时，signal 归零可能产生随机噪声，导致舵机关断前跳动；项目 ChangeLog 还记录了 PWM channel conflict、detached servo 和 re-attach 修复。这些是成熟项目的实测故障线索，不是 ESP32-S3 或本项目 CH32 的直接失效证明。

可移植结论：

1. 保留 `CH32V003F4P6 + 74HC244`，不因 ESP32-S3 理论上有 8 路 LEDC 就收回 8 个 GPIO或丢掉独立故障域。
2. EDD-015 已有 100 ms watchdog、atomic commit、range reject 和 `/OE` 默认高；还应把“关断时不得产生 500–2500 µs 范围内的残余脉冲”写入 PVR/bench criterion。
3. `74HC244` `/OE` 必须先硬件禁用，再停 CH32 timer/清 pin；重新启动时先配置全部 inactive level，再统一使能。只把 pulse width 写为 0 不能视为安全关断。
4. 首板必须测两颗 timer 的相位、更新瞬间、watchdog、CH32 reset/SWIO烧录、ESP I2C reset和 ESTOP 同时发生时的八路波形。

### 5.2 软件/接口经验

可迁移但不改变当前游戏控制边界：

- 每个舵机保存独立方向、零位、角度范围和 min/max pulse；不要在 control FSM 中硬编码。
- 单独提供 calibration mode、all-off、single-channel-off 和 pulse-width measurement test。
- 低电压处理顺序应是撤销新运动、进入已定义安全姿态或直接 safe stop、禁 PWM、断 servo power；具体行为服从牛来快跑 v0 FSM和安全边界，不能照搬 OpenCat 自动恢复动作。
- IMU、BLE、servo、voice/extension 的初始化顺序会影响稳定性；本项目应在 firmware bring-up matrix 中逐项启用，而不是全外设一次启动。

## 6. CH32 PWM 与 PCA9685 模块对比

Adafruit PCA9685 servo module 和 OpenCat 的 PCA backend 是成熟参考，最有价值的不是要求换回 PCA9685，而是提供兼容性对照：

| 项目 | PCA9685 成熟模块 | 当前 CH32 engine | 应迁移的要求 |
| --- | --- | --- | --- |
| host interface | I2C，常用地址 0x40 | I2C 0x40 | 保留简洁、批量更新的 host adapter |
| PWM update | 多寄存器、auto-increment | shadow + `0xA5` atomic commit | CH32 atomic commit 比逐路生效更适合四足同步更新 |
| pulse/frequency accuracy | 内部 oscillator需校准，模块库明确提醒实测 | HSI + TIM1/TIM2，仍待实测 | 两者都不能以 nominal clock 宣称 pulse accuracy |
| all-off | PCA full-off bit | 74HC244 `/OE` + timer disable | 硬件 `/OE` 必须为最终安全层 |
| failure mode | I2C/oscillator/register state | firmware/watchdog/reset | 上电、通信丢失和 host reset必须有确定状态 |

因此 PCA9685 保留为 bring-up 对照板或回滚候选即可，不回到当前 BOM；CH32 节省成本的结论保持，但必须支付一次 firmware、programming 和 waveform validation 成本。

## 7. SY8388 与高电流 buck 模块

### 7.1 exact EVB/资料的直接价值

Silergy 官方产品页现列出 `EVB_SY8388ARHC`，用于连续 8 A、峰值 12 A、24 V 输入的评估。当前公开 Rev.1.0 datasheet还给出：

- 内置 20/10 mΩ top/bottom switch；
- 4–24 V operating range、600 kHz、internal soft-start、PG、output discharge；
- ILMT Low/Floating/High 对应不同 valley current limit；
- QFN2.5 × 2.5 mm，datasheet热阻基于 8.5 × 8.5 cm、四层、2 oz Silergy EVB；
- layout section和典型功率回路图。

当前 EDD-024 的 2.7 µH、有效 CIN ≥40 µF、有效 COUT ≥330 µF 是针对 9–12.6 V→6 V舵机动态重新计算的项目值，不应被典型 3.3/5 V EVB值覆盖。

### 7.2 同功能电源模块能迁移什么

Pololu 等成熟高电流 regulator module通常公开连续/短时电流随输入、温度和散热变化的曲线，并把 enable、反接、hiccup、UVLO、温度保护分别说明。其最重要的可移植原则是：

- “8 A switch/peak”与“8 A continuous output”必须分开；
- enable 是否真正切断输出、是否存在 input-to-output path要逐项确认；
- 保护恢复是 latch、auto-retry还是 hiccup会改变机器人故障行为；
- 模块额定值受板面积、铜厚、气流和端子限制，不能迁移为本板能力。

### 7.3 对当前设计的具体建议

1. 向 Silergy 获取/保存 `EVB_SY8388ARHC` 原理图、BOM、Gerber/layout或至少高清 layout；当前仅有产品页、照片和 datasheet不足以称为“完整下载的 EVB design package”。
2. PCB layout必须把 Silergy power-stage placement作为几何起点；允许改变外围数量，不允许改变 hot-loop相对位置后再以 datasheet thermal number证明。
3. 8.5 × 8.5 cm、四层、2 oz EVB 与本板75 × 60 mm仍不同；PVR-015需测 IC、电感、输入/输出电容、连接器与板铜温度。
4. 官方 8 A/12 A EVB headline不关闭 6 A连续、8 A/1 s、regen、ESTOP、audio/IMU/USB EMI或封闭机身热验证。

## 8. USB POWER 与逻辑 source mux

### 8.1 TUSB320EVM

TI `TUSB320EVM` 支持 UFP/DFP/DRP、GPIO/I2C mode，并公开 user guide。对本项目可迁移的是：

- UFP CC connection和current advertisement状态观察；
- controller由连接端 VBUS/AON domain先行供电；
- GPIO status在主逻辑未启动时也能决定允许的电源状态；
- 为 CC、VBUS和状态脚提供测试点。

不能迁移的是 EVM 上为通用评估设置的 DIP switch、DFP/DRP path、legacy connector和 I2C配置。当前设计只需 sink-only UFP GPIO mode，应固定 straps，减少跳线和误配置点。

需要在 pin/state table 中显式证明：未连接、Default、1.5 A、3 A、VBUS brownout、controller reset和 CC 翻转插入的全部状态；不能只验证稳定3 A适配器。

### 8.2 TPS2121EVM-023

TI EVM user guide包含 schematic、layout、BOM、priority、OVP、soft-start/input settling和 ILM设置。可迁移：

- HotRod/VSON 焊盘与散热布局；
- IN1/IN2/OUT局部去耦；
- PR1/CP2、OV1/OV2、SS、ILM和ST test point；
- 逐种输入组合测试 priority和切换。

本项目不要求无缝运行，已经接受 source transfer时 supervisor-controlled reset，因此不能直接复制 EVM 的seamless配置；必须按USB优先、battery fallback、3.40 V窗口和最终3V3 buck输入动态重新配置。

低成本结论：在功能不变的前提下，完整 TPS2121 已把两个输入开关、reverse blocking、priority和current limit集成在一颗器件中。除非替代方案的**总 BOM + 扩展库贴装费 + 验证成本**明确更低，否则不应拆成controller、四颗FET和自制interlock。

## 9. LM74800、TVS 与当前 blockers

TI `LM74800EVM-CS/CD` 展示 back-to-back MOSFET、reverse battery、reverse-current blocking、OV cutoff/clamp和 enable控制。EVM默认面向24 V汽车系统，并使用高压FET；它的器件值和浪涌目标不能直接迁移到3S LiPo。

可迁移：

- `QDIODE + QLOAD` common-source topology；
- DGATE/HGATE loop、gate resistor和本地去耦布局；
- input/output lead inductance在快速关断时分别产生正/负瞬态，必须连同TVS评估；
- 在不同 VIN/VOUT斜率、source sink能力和负载下测reverse behavior。

不能迁移或关闭：

- EVM演示“能阻断反向电流”不等于制造商保证本项目要求的`≤100 µA` steady OUT→BAT leakage；v0.7保留该数值，并以首批逐板25/40 °C EOL而不是EVM关闭CDB-005。
- 汽车load dump TVS和3S LiPo线束/舵机regen瞬态不同；DIB-013继续 OPEN。
- EVM 2 A级默认功率器件、200 V/60 V FET和板面积不适合本项目6 A路径的成本/热结论。

下一步应优先向TI获取 LM74800 在 `BAT=9–12.6 V, OUT slightly above BAT, EN state, temperature` 条件下的guaranteed leakage解释；若只有typical/demo数据，就必须放宽Design Input或换有明确保证的器件，不能用EVM测试替代datasheet guarantee。

## 10. Current sense、audio 与传感模块

### 10.1 INA180/181 EVM

TI明确说明 `INA180-181EVM`用于功能评估，**其fixture layout不是目标电路或EMC layout模板**。因此：

- 可迁移 gain、unidirectional/bidirectional、REF配置和test-point方法；
- 不能复制EVM走线来证明5 mΩ shunt精度；
- 本板继续使用shunt四线Kelvin、输入对称、远离SW和servo connector大电流回路；
- 单向方案不记录regen current，PVR中必须同时用示波器/电流探头观察负向事件。

### 10.2 MAX98357 模块

[Adafruit MAX98357 I2S Amp Breakout](https://github.com/adafruit/Adafruit-MAX98357-I2S-Amp-Breakout)公开 schematic和board文件，是成熟同功能模块。可迁移：

- I2S BCLK/LRCLK/DIN三线连接；
- BTL `OUTP/OUTN`成对走线，任何一端都不接地；
- SD/GAIN strap、local bulk/decoupling和filterless最小实现；
- 功放远离 microphone 与 RF，扬声器线作为开关节点处理。

不能直接复制：该breakout主要以5 V、TQFN和独立小板条件展示性能；本项目为3.40 V common rail、`MAX98357AEWL+T` WLP且与buck/servo/麦克风共板。其3.2 W@5 V宣传值不能作为本板4 Ω/2 W声学或热证据。EDD-019保持，PVR-004/006继续存在。

### 10.3 IMU、Hall、WS2812、IR和屏幕模块

- OpenCat对ICM42670的支持说明其driver、自检、校准、offset存储和初始化顺序可迁移；电气连接仍只采用 exact `ICM-42670-L`资料。
- 通用Hall breakout通常含5 V supply、比较器或不同滤波，不能替代HAL49ESO 3.3/3.4 V零点/灵敏度验证；EDD-017与PVR-008保持。
- NeoPixel模块支持per-pixel decoupling、chain isolation和pattern soak-test思路；不能证明本板3.30 V noisy rail的minimum margin。EDD-020与PVR-009保持。
- IR receiver/transmitter模块可用于窗口、方向、阳光/灯光和servo noise A/B实验；不能替代exact receiver supply/filter与外壳验证。
- 未取得 exact ST7789 module SKU、pinout、背光接法和峰值电流前，任何“同为ST7789”的模块都只是mechanical/software reference；PVR-010保持。

## 11. 不减功能的低成本调整

这些调整不改变v0.6功能或安全语义：

1. **固定官方CAD与reference版本**：减少重画footprint、USB/antenna返工；成本收益主要来自避免首板报废。
2. **保持CH32方案**：当前两器件价格显著低于PCA9685；以测试时间换料价是已接受策略，但必须完成waveform和programming验证。
3. **exact功能器件优先集成方案**：TPS2121这类集成mux只有在分立替代的全成本明确更低时才更换；不能只比单颗芯片价。
4. **用DNP控制首板装配，不删除能力**：Hall2–4、NTC、可选EMI器件和调试straps保持layout/接口，首板按当前EDD默认DNP；功能路径仍在。
5. **共用阻容与ESD料号**：在不改变额定、介质、DC-bias、容差和封装要求时，优先复用JLC basic parts，减少unique extended MPN和换料费。
6. **模块只用于台架A/B reference**：PCA9685、MAX98357、IMU、Type-C或电源EVM用于对照波形/软件，不嵌入量产板，不把模块成本带入PCBA。
7. **分阶段装配同一完整PCB**：首板先装AON/3V3/ESP/USB，再装CH32和低压外设，最后装SY8388/高电流路径；不是删除功能，而是降低一次性bring-up故障面。
8. **避免过时文档诱发重复设计**：`HARDWARE_INTERCONNECT_SPEC_v0.1.md`仍描述外置UBEC，Decision 008仍引用旧Freeze；在进入结构/原理图前必须同步到v0.6，否则会浪费连接器、板面积和review时间。

## 12. 对当前 EDD / blocker 的建议动作

| 优先级 | 动作 | 影响 |
| --- | --- | --- |
| P0 | 获取并固定 `EVB_SY8388ARHC` design package；将EVB与EDD-024逐项diff | 关闭“没有成熟buck reference”的过程缺口；不自动关闭PVR-015 |
| P0 | 完成LM74800 leakage budget、fixture/state matrix并逐板执行v0.7 EOL | CDB-005已转system acceptance；PVR-017在首批放行前必须PASS |
| P0 | 由Operator冻结DIB-013的实际3S线束/热插拔/regen transient envelope | 才能选入口TVS并完成LM74800/MOSFET stress |
| P1 | 以TPS2121EVM/TUSB320EVM逐脚对照EDD-009/010 | 收敛exact suffix、straps、test point和default-state table |
| P1 | 把OpenCat暴露的shutdown pulse/jump问题加入EDD-015 PVR | 补足CH32+74HC244关断/复位验证，不改硬件功能 |
| P1 | 按官方footprint/STEP做75 × 60 mm placement feasibility | 在原理图前发现天线、USB、WLP、IMU、麦克风和功率区冲突 |
| P2 | 用MAX98357和PCA模块建立firmware/波形A/B台架 | 软件可在主板到货前验证；模块结果不代替本板PVR |

## 13. 仍未完成

- 尚未取得 Silergy `EVB_SY8388ARHC` 的完整原理图/BOM/Gerber/layout下载包；官网链接存在但本轮缓存获取失败。
- 尚未找到公开、可核实且覆盖本项目全部功能和电源故障语义的单块四足控制器原理图；因此采用“官方block reference +成熟系统module behavior”组合方法。
- 未完成exact 3V3 buck、USB front-end/mux、servo isolation/current sense、安全逻辑、保险丝及holder的最终MPN审阅。
- 未修改任何 Design Input、EDD状态或DIB/CDB状态；新增PVR-016，把成熟项目的PWM关断故障线索转成当前架构的验证要求，但没有将其写成CH32已失效的结论。
- 未执行 schematic、ERC、netlist、layout、采购或实体测试。
