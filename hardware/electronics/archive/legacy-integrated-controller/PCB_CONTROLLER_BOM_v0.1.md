# 牛来快跑 PCB Controller 计算型参考 BOM v0.1

状态：**Historical 2S/5 A calculation / 已被 3S/6 A 设计输入冻结取代，不得直接用于新原理图**
日期：2026-08-30
当前冻结输入：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md)；本历史 BOM 已被取代。
历史系统基线：[`PCB_CONTROLLER_BASELINE_v0.1.md`](PCB_CONTROLLER_BASELINE_v0.1.md)
相关决定：[`Decision 008`](../../../../docs/decisions/008-pcb-controller-v0.1.md)

## 1. 使用边界

这是一份按早期 2S/5 A 假设计算的历史参考 BOM。2026-08-30 接受的 3S 电池、6 A 连续/8 A 短时舵机母线、主板供给 UBEC 输入、主开关/急停和 GPIO 变化已记录在当前冻结输入中。本文件中的电池阈值、TVS、ADC 分压、5.5 A 电源路径器件和相关外围值必须重新设计，不能直接抄入新原理图。它也不能单独证明成板正确；量产/下单前仍必须完成原理图审查与 ERC、封装和引脚逐项核对、PCB 电流/温升检查、USB 走线检查以及样机台架验证。

- 保留了 Operator 已填写的 A01-A09 嘉立创/LCSC 数据；右侧空栏留给后续低价替代。
- “基准器件/MPN”是当前电气计算所依据的器件。替代件不能只按功能名替换，必须同时核对数据手册、完整后缀、引脚、封装、绝对最大额定值、保护行为和外围网络。
- 默认电池是带合适保护/保险措施的 2S 锂电池，工作范围 6.0-8.4 V；本板**不集成 2S 充电器或电芯均衡器**。
- 外置 UBEC 输入是已稳压的 6 V/5 A 舵机电源；八个 MG90S 的真实同时堵转电流未知，因此 5 A 是试验上限而不是“八路均可同时堵转”的保证。
- USB POWER 口必须配 5 V/3 A 电源和额定 3 A 的线。USB 模式的 3 A 是整板总输入预算，不是全部留给舵机。
- USB DATA 口只负责原生 USB 2.0 烧录、USB CDC/JTAG 和通信，不给整板供电；其 VBUS 仅用于插入检测。
- MAX98357A 在 3.3 V 下驱动单只 4 Ω/2 W 扬声器。它不会在 3.3 V 电源下持续输出 2 W；这是有意采用的安全/功耗折中。

## 2. 已固化的电源和接口拓扑

```text
2S VBAT -> 电池 eFuse/反接保护 -----> TPS2121 IN1 --+
                                                      +-> TPS54302 3.3 V/3 A -> MCU/传感器/音频/扩展
USB-C POWER -> 小 LDO -> TUSB320 + 3A 硬件译码 -> USB eFuse EN
       |                                      |
       +-----------> USB eFuse ---------------+-> TPS2121 IN2
                              |
                              +-> LM73100（固件允许）----------+
外置 6 V/5 A UBEC --------------------> LM73100（固件允许）----+-> 5 mΩ -> SERVO_V+

USB-C DATA -> ESD -> GPIO19 D- / GPIO20 D+；VBUS 不接系统电源
```

原始 USB VBUS 只先供给毫安级 Type-C 检测域；只有检测到 3 A 宣告后，USB eFuse 才允许整板从该口取电。两只 LM73100 提供真正的反向电流阻断，因此 5 V USB 和 6 V UBEC 不会硬并联；两者同时存在时，较高的 6 V UBEC 自然承担舵机负载。所有电源地共地。

### 2.1 3.3 V 负载预算

| 负载 | 设计预算 | 说明 |
| --- | ---: | --- |
| ESP32-S3-WROOM-1-N16R8 | 0.50 A 峰值预算 | 覆盖 Wi-Fi/BLE 发射脉冲，不以平均值选电源 |
| MAX98357A + 4 Ω 扬声器 | 0.70 A 峰值预算 | 3.3 V、限制音量；音频峰值而非连续直流 |
| 4 × WS2812C-2020-V6 | 0.06 A | 每色 5 mA、每颗最多约 15 mA；固件仍限制总亮度 |
| ESP 外的数字/模拟器件 | 0.10 A | PCA9685、IMU、麦克风、霍尔、IR、逻辑门等 |
| 外接屏幕和 3V3 扩展 | 0.50 A | 经 0.75 A PTC；PTC 不是精密限流器，实际屏幕浪涌需在采购后复核 |
| **设计峰值合计** | **约 1.86 A** | 给 3 A buck 留出启动、温升和容差余量 |

TPS54302 采用 TI 3.3 V 参考值：6.8 µH、2 × 22 µF 输出、100 kΩ/22.1 kΩ 反馈。3 A 是芯片额定能力，最终可用电流仍取决于电感、陶瓷电容直流偏压、铺铜和环境温度。

在 USB POWER 模式，逻辑支路按 3.3 V × 1.86 A / 约 90% / 5 V ≈ 1.36 A 输入估算，3 A 总预算只剩约 1.6 A 给舵机。固件必须将 USB 舵机模式定义为调试/降载模式：错峰启动、限制同时运动、检测欠压和过流；正常八舵机运动优先使用 6 V/5 A UBEC。

### 2.2 已计算的保护与测量值

- 电池 eFuse：R1/R2/R3 = 412 kΩ/25.5 kΩ/67.3 kΩ，按 TPS25947 阈值计算，典型 UVLO 上升约 6.53 V、下降约 5.93 V，OVLO 约 9.00 V。计入数据手册阈值全温范围与 1% 电阻后，三者估算边界分别约 6.33-6.76 V、5.76-6.17 V、8.72-9.33 V。RILM = 1.33 kΩ，典型限流约 2.51 A，器件 ±10% 规格对应约 2.26-2.76 A。
- USB eFuse：EN/UVLO 由 `USB_3A_OK` 直接使能，未宣告 3 A 时保持关闭；OVLO 分压 383 kΩ/100 kΩ，典型约 5.80 V，计入阈值与 1% 电阻约为 5.63-6.00 V；RILM = 1.10 kΩ，典型限流约 3.03 A，±10% 约为 2.73-3.33 A。低压由器件内部 UVP 和下游 buck UVLO 共同兜底。
- TPS2121：USB 优先。电池 PR1 分压 33 kΩ/10 kΩ，8.4 V 时为 1.95 V；USB CP2 分压 10 kΩ/10 kΩ，5 V 时为 2.50 V。OV1 = 75 kΩ/10 kΩ，典型约 9.01 V；OV2 = 44.2 kΩ/10 kΩ，典型约 5.75 V。RILM = 29.8 kΩ，官方全温规格为 3.0-4.0 A、典型 3.5 A。
- 舵机电流：5 mΩ 分流器配 INA181A2（50 V/V），换算为 `VOUT = 0.25 V/A`；5 A = 1.25 V，10 A = 2.50 V。分流器在 5 A 时耗散 0.125 W、10 A 时 0.50 W，1 W 等级留有脉冲余量。
- 电池 ADC：从电池 eFuse 输出 `VBAT_PROT` 取样，采用 200 kΩ/100 kΩ，8.4 V 映射为 2.80 V，静态电流 28 µA；eFuse 关断后 ADC 不会被原始电池反灌；必须使用 ADC1 引脚并做实际 ADC 校准。
- 舵机母线：4 × 220 µF = 880 µF 低 ESR 聚合物电容；LM73100 的 3.3 nF 软启动电容将母线爬升控制在约 10 ms 数量级，使单纯电容充电浪涌约为 0.5 A 数量级。

## 3. 主 IC、传感器和功能器件

| Item | 参考位号 | Qty | 基准制造商 / 完整 MPN | 封装 | 电源 / 接口 | 不可放宽的验收条件 | JLC/LCSC part no. | Price break | Stock / Basic-Extended | Operator proposed substitute | Operator notes |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A01 | U1 | 1 | Espressif `ESP32-S3-WROOM-1-N16R8` | Module | 3V3；USB/BLE/GPIO | 必须是 N16R8；GPIO35-37 被 Octal PSRAM 占用；天线净空按官方要求 | C2913202 | ￥31.6 | stock |  |  |
| A02 | U2 | 1 | NXP `PCA9685PW,118` | TSSOP-28 | 3V3；I2C；8 PWM | OE 由 10 kΩ 上拉并由 TCA9539 P15 拉低使能；A0-A5 接地；只使用 8 路但保留其余测试点可选 | C2678753 | ￥20.39 |  |  |  |
| A03 | U3 | 1 | TDK InvenSense `ICM-42670-L` | LGA-14 | 3V3；I2C；INT1 -> TCA P16 | 精确 L 后缀；核对 pin 1 和官方 land pattern；读取 WHO_AM_I 后才允许运动功能 | C49073546 | ￥7 |  |  |  |
| A04 | U4 | 1 | LinkMems `LMD3526B261-OFA03` | 3.5 × 2.65 mm、5-pad、bottom-port | 3V3；PDM CLK/DATA | 1.62-3.6 V、典型 65 dBA SNR、约 750 µA；底部声孔必须有 PCB 开孔、禁布区和防污染措施；L/R 用双焊盘 0 Ω 选择 | C41347997 | ￥2.4 |  |  |  |
| A05 | U5 | 1 | Analog Devices `MAX98357AEWL+T` | WLP-9, 0.4 mm | 3V3；I2S；BTL | 必须按 WLP 官方焊盘；扬声器两端均不可接地；3.3 V 工作；SD_MODE 可由 TCA9539 强制关闭 | C2682619 | ￥3.01 |  |  |  |
| A06 | U6 | 1 | TI `TUSB320LIRWBR` | X2QFN-12 | 3V3_CC；CC；GPIO mode | VDD 接专用 3V3_CC，VBUS_DET 经 900 kΩ 接 USB5_RAW；PORT=UFP、ADDR 浮空、ID/OUT3 NC、EN_N 拉低；OUT1/OUT2 拉到同一 3V3_CC | C2836598 | ￥4.83 |  |  |  |
| A07 | U7 | 1 | TI `INA181A2IDBVR` | SOT-23-6 | 3V3；高边分流；ADC1 | A2 = 50 V/V；输入做真正 Kelvin；输出 1 kΩ/100 nF 后进 ADC1 | C2058784 | ￥2.0707 |  |  |  |
| A08 | U8-U11 | 4 | Hallwee `HAL49ESO` | SOT-23-3L | 3V3；4 × ADC1 | 当前依据：3-6.5 V、约 3 mA、约 2 mV/Gs、±100 mT；采购前再次核对 1=VDD、2=OUT、3=GND | C7420984 | ￥0.7052 |  |  |  |
| A09 | LED1-LED4 | 4 | Worldsemi `WS2812C-2020-V6` | 2020 | 3V3；单线串联 | C55109522 必须对应 V6；3.3-5.5 V、5 mA/色；每颗 100 nF；若换 12 mA/色的 B-2020-V6，四颗峰值由 60 mA 增至 144 mA | C55109522 | ￥0.6074 |  | `WS2812B-2020-V6` |  |
| A10 | U12 | 1 | TI `TCA9539PWR` | TSSOP-24 | 3V3；I2C；INT；RESET | P00-P03=脚部开关，P04=USB_3A_OK_N，P05=USB eFuse FLT，P06=电池 eFuse FLT，P07=USB 舵机命令，P10=DATA_VBUS_N，P11=功放 SD_MODE，P12=屏幕 BL，P13=UBEC 舵机命令，P14=MUX_ST，P15=PCA OE_N，P16=IMU INT1，P17=预留；A0/A1 接地；RESET_N 由 GPIO3 武装 |  |  |  |  |  |
| A11 | U13 | 1 | TI `SN74LVC1G02DBVR` | SOT-23-5 | 3V3_CC | 对 TUSB320 OUT1/OUT2 做 NOR；仅 L/L（3 A）输出高，同时使能 USB eFuse |  |  |  |  |  |
| A12 | U14 | 1 | TI `TPS70933DBVR` | SOT-23-5 | USB5_RAW -> 3V3_CC | 150 mA LDO，实际只供 TUSB320/U13；2.7-30 V 输入且带反向电流保护，可承受 eFuse 前的 VBUS 异常；固定 3.3 V；EN 直接接 USB5_RAW |  |  |  |  |  |
| A13 | IRRX1-IRRX4 | 4 | Vishay `TSOP38438` | Through-hole | 3V3；4 × RMT/GPIO | 38 kHz、940 nm、3.3 V 可用；1=OUT、2=GND、3=VS；接收窗朝四侧；实际日光测试后再冻结 |  |  |  | `VS1838B` 等低价件待测 |  |
| A14 | D_IR_TX | 1 | Vishay `VSMY2943SLX01` | Side-view SMD | 3V3 脉冲 | 940 nm、侧发光；目标脉冲约 80-100 mA；协议、占空比和外壳窗口共同限制平均功耗 |  |  |  |  |  |
| A15 | Q_IR_TX | 1 | Alpha & Omega `AO3400A` | SOT-23 | 3V3 GPIO 低边驱动 | 在 VGS=2.5/3.3 V 有明确低 RDS(on) 规格；栅极 33 Ω、100 kΩ 下拉 |  |  |  |  |  |
| A16 | Q_DATA_SENSE | 1 | Nexperia `BSS138P,215` | SOT-23 | DATA VBUS 到 3V3 逻辑隔离 | VBUS 驱动栅极，源极接地，漏极由 10 kΩ 拉到 3V3 并进入 TCA P10；防止 DATA 口给未上电 3V3 反灌 |  |  |  |  |  |
| A17 | Q_USB_OK | 1 | Nexperia `BSS138P,215` | SOT-23 | 3V3_CC 到板载 3V3 状态隔离 | 栅极由 USB_3A_OK 驱动，漏极由 10 kΩ 拉到板载 3V3；TCA P04 读取低电平表示 3 A；避免两电源域互相反灌 |  |  |  |  |  |

## 4. 电源、保护和高电流器件

| Item | 参考位号 | Qty | 基准制造商 / 完整 MPN | 封装 | 功能 | 关键额定值 / 设计要求 | JLC/LCSC part no. | Price break | Stock / Basic-Extended | Operator proposed substitute | Operator notes |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| B01 | U15 | 1 | TI `TPS54302DDCR` | SOT-23-6 | 3V3 buck | 4.5-28 V 输入、3 A、同步降压；按第 6 节固定外围值 |  |  |  |  |  |
| B02 | L1 | 1 | Bourns `SRP5030TA-6R8M` | 5.7 × 5.2 × 2.8 mm | buck 电感 | 6.8 µH ±20%；屏蔽；官方 Irms 4.0 A、Isat 4.3 A、DCR 最大 76.2 mΩ；替代后重算纹波、饱和和温升 |  |  |  |  |  |
| B03 | U16 | 1 | TI `TPS2121RUXR` | VQFN-HR-12 | 电池/USB 逻辑电源 mux | 2.8-22 V、4.5 A；USB 优先；防反灌；按官方 PowerPAD/铜皮要求散热 |  |  |  |  |  |
| B04 | U17,U18 | 2 | TI `TPS259470LRPWR` | VQFN-10 | 电池/USB eFuse | 2.7-23 V；反向极性保护、UV/OV、限流、过温和故障输出；L=latch-off |  |  |  |  |  |
| B05 | U19,U20 | 2 | TI `LM73100RPWR` | HotRod QFN-10 | USB/UBEC 理想二极管 OR | 2.7-23 V、5.5 A 连续、真正反向电流阻断；高电流焊盘和散热铜按官方 layout |  |  |  |  |  |
| B06 | RSH1 | 1 | Vishay `WSL2512R0050FEA` | 2512 | 舵机电流分流 | 5 mΩ、1%、1 W、低温漂；四线 Kelvin 走线，不得从大电流焊盘任意点取样 |  |  |  |  |  |
| B07 | C_SER1-C_SER4 | 4 | Panasonic `10SVPE220M` | 6.3 × 5.9 mm SMD OS-CON | 舵机母线储能 | 220 µF ±20%、10 V、20 mΩ ESR、2.7 A rms；在八个舵机接口附近分布放置；严格核对极性 |  |  |  |  |  |
| B08 | D_SER | 1 | Diodes Inc. `B540C-13-F` | SMC | 舵机负向瞬态钳位 | 5 A/40 V 肖特基；阳极接 GND、阴极接 SERVO_V+；不替代良好回流路径 |  |  |  |  |  |
| B09 | F_3V3_EXT | 1 | Bourns `MF-PSMF075X-2` | 0805 | 外接 3V3 故障保护 | 0.75 A hold、6 V PTC；温度升高时保持电流下降；外设设计预算仍为 0.50 A；仅串在扩展口 |  |  |  |  |  |
| B10 | D_VBUS_PWR,D_VBUS_DATA | 2 | Littelfuse `SMF5.0A` | SOD-123FL | 两个 VBUS 各自 TVS | VRWM 5 V；每个 USB 连接器各一只，回地最短；POWER 侧与 eFuse、DATA 侧与感测 MOSFET分别联合检查 |  |  |  |  |  |
| B11 | D_ESD_DATA | 1 | TI `TPD4E05U06DQAR` | USON-10 | DATA D+/D-/CC1/CC2 ESD | 4 通道、USB 2.0 低电容、流穿式布线 |  |  |  |  |  |
| B12 | D_ESD_PWR | 1 | TI `TPD2E2U06QDBZR` | SOT-23-3 | POWER CC1/CC2 ESD | 2 通道、低电容；VBUS 另由 B10 保护 |  |  |  |  |  |
| B13 | R_SPK1,R_SPK2 | 2 | Yageo `RC0805JR-070RL` | 0805 | 扬声器 EMI 跳线 | 基线装 0 Ω；保留同封装磁珠替换位，实测 EMI 不合格后才评估 `BLM21PG221SN1D`；两根 BTL 线必须对称修改 |  |  |  | Murata `BLM21PG221SN1D` |  |
| B14 | D_VBAT | 1 | Littelfuse `SMBJ12CA` | SMB | 电池输入双向 TVS | VRWM 12 V；双向器件避免反接时直接短路；钳位需保持在 eFuse 28 V 绝对最大值以下 |  |  |  |  |  |

## 5. 连接器、开关和外接器件

| Item | 参考位号 | Qty | 基准器件 / 要求 | 信号 | 关键要求 | JLC/LCSC part no. | Price break | Stock / Basic-Extended | Operator proposed substitute | Operator notes |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| C01 | J_USB_PWR,J_USB_DATA | 2 | GCT `USB4105-GF-A-120` | 两个 USB-C | 16-pin USB 2.0；VBUS 触点组官方额定 5 A；1.20 mm shell stake；最终封装必须由该后缀图纸建立，不能套用“通用 Type-C” |  |  |  |  |  |
| C02 | J_BAT | 1 | AMASS `XT30PW-M` | VBAT/GND | 板上仅承载逻辑支路；电池用外部 Y 线同时供板和 UBEC；丝印明确极性 |  |  |  |  |  |
| C03 | J_UBEC | 1 | JST `B2P-VH(LF)(SN)` | 6V_UBEC/GND | JST-VH 3.96 mm；连接器/线材连续额定 ≥5 A；与电池接口机械防呆不同 |  |  |  |  |  |
| C04 | J_S1-J_S8 | 8 | Harwin `M20-9990345` 或等效 1×3 2.54 mm 排针 | GND/SERVO_V+/PWM | 与 MG90S 母头兼容；每触点 ≥3 A；统一 G-V-S 顺序并在丝印标注 |  |  |  |  |  |
| C05 | J_F1-J_F4 | 4 | JST `B2B-PH-K-S(LF)(SN)` | GND/SW | 2.0 mm 带摩擦锁；KW10 裸线在线束端压接 PH 外壳；每路硬件 RC + 软件去抖 |  |  |  |  |  |
| C06 | J_SPK | 1 | JST `B2B-PH-K-S(LF)(SN)` | SPK+/SPK- | 2-pin 带锁；BTL 两线都不得接机壳/地；标注扬声器极性便于一致装配 |  |  |  |  |  |
| C07 | J_CTRL | 1 | JST `B4B-PH-K-S(LF)(SN)` | 3V3/GND/UART_TX/UART_RX | 专用 3.3 V TTL 控制 UART；TX/RX 按“本板视角”丝印；非 RS-232、非 5 V 容忍 |  |  |  |  |  |
| C08 | J_I2C | 1 | JST `BM05B-SRSS-TB(LF)(SN)` | 3V3/GND/SDA/SCL/INT | 1.0 mm JST-SH；只用于短线 3.3 V 外设；外部模块默认不再装强上拉 |  |  |  |  |  |
| C09 | J_SENSOR1,J_SENSOR2 | 2 | JST `B4B-PH-K-S(LF)(SN)` | 3V3/GND/GPIO/ADC-GPIO | 通用低速传感器；不是 5/6 V 输出口；外接长线时按实际环境补 ESD |  |  |  |  |  |
| C10 | J_GPIO_A,J_GPIO_B | 2 | Harwin `M20-7821046`，1×10、2.54 mm 直插排母 | 空闲/共享 GPIO、3V3、GND | 每触点 3 A；两条独立 1×10 便于布置；引出全部 5 个空闲 ESP GPIO，满足屏幕 SCLK/MOSI/CS/DC/RST；TCA P12 作 BL；重复 3V3/GND |  |  |  |  |  |
| C11 | SW_BOOT,SW_RESET | 2 | C&K `PTS636SK50JSMTRLFS` | GPIO0/GND；EN/GND | 6.0 × 3.5 mm、SMT、SPST-NO、顶部按压、5.0 mm 高；装配后可触达且不位于机器人正常受力位置 |  |  |  |  |  |
| C12 | TP_USB | 4 | 1.0-1.5 mm 测试盘 | D+/D-/DATA_VBUS/GND | D+/D- 测试盘不得形成长支线 |  |  |  |  |  |
| C13 | TP_RECOVERY | 4 | 1.0-1.5 mm 测试盘 | GPIO0/EN/UART0_TX/UART0_RX | 即使 USB 失败仍能恢复；不要把 UART0 当成对外控制 UART |  |  |  |  |  |
| C14 | TP_PWR | 7+ | 1.5-2.0 mm 测试盘 | VBAT、USB5_RAW、USB5_PROT、3V3、UBEC6、SERVO_V+、GND | 允许安全夹表和示波器测量；每个关键地附近提供 GND 测试点 |  |  |  |  |  |
| C15 | F01 off-board | 1 | 2S 锂电池，6.0-8.4 V | 系统电源 | 容量、放电倍率、保护板、XT30、尺寸和质量由 Operator 固化 |  |  |  |  |  |
| C16 | F02 off-board | 1 | 外置 6 V/5 A UBEC | 舵机电源 | 输入覆盖 2S；连续 5 A 需台架验证；输出不得超过舵机可接受电压 |  |  |  |  |  |
| C17 | F03 off-board | 8 | MG90S 候选舵机 | PWM/6 V/GND | 同批次实测空载、运动和堵转电流；开始实体测试前设置限位、固定和急停 |  |  |  |  |  |
| C18 | F04 off-board | 4 | KW10 脚部开关 + PH2 线束 | 开关到地 | 裸线压接后做拉力/短路检查；机械安装不能让线材承受脚部冲击 |  |  |  |  |  |
| C19 | F05 off-board | 1 | 已封装 170×320 ST7789 屏幕 | SPI + 控制 + 3V3/GND | 下单前必须取得模块 SKU、引脚、电流和“是否自带 LDO/电平转换”；本基线按 3.3 V 逻辑 |  |  |  |  |  |
| C20 | F06 off-board | 1 | 3516、4 Ω/2 W 扬声器 + PH2 线束 | BTL 音频 | 单扬声器；裸线压接 PH2；线长尽量短并双绞；音腔决定实际响度/音质 |  |  |  |  |  |
| C21 | F07 off-board | 1 set | 5 V/3 A USB-C 电源和 3 A 线 | POWER 口 | 必须能在 CC 上宣告 3 A；普通“能充电”的线不等于 3 A 合格线 |  |  |  |  |  |
| C22 | F08 off-board | 1 | USB 2.0 Type-C 数据线 | DATA 口 | 必须含数据线芯，不能是 charge-only |  |  |  |  |  |
| C23 | F09 off-board | 1 | 外置 2S 平衡充电器 | 电池充电 | 必须匹配电池化学体系和 2S；充电时默认从机器人断开 |  |  |  |  |  |

## 6. 电源关键外围网络

以下数值是原理图输入，不再以“TBD 电阻/电容”处理。普通电阻基准为 0603、1%、≥50 V；阈值/反馈建议 1%。陶瓷电容默认 X7R/X5R，并按工作电压下的有效容量核对，不能只看标称容量。

| Item | 参考位号 / 网络 | Qty | 数值与基准 | 作用 / 布局要求 | JLC/LCSC part no. | Price break | Stock / Basic-Extended | Operator proposed substitute | Operator notes |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- |
| P01 | C15_IN1,C15_IN2 | 2 | 10 µF, 25 V, X7R, 1206 | TPS54302 输入；紧贴 VIN/PGND |  |  |  |  |  |
| P02 | C15_INHF | 1 | 100 nF, 25 V, X7R, 0603 | TPS54302 高频输入旁路 |  |  |  |  |  |
| P03 | C15_OUT1,C15_OUT2 | 2 | 22 µF, 10 V, X7R, 1206 | TPS54302 输出；必须核对 3.3 V 下有效容量，总有效值接近参考设计 |  |  |  |  |  |
| P04 | C15_BOOT | 1 | 100 nF, 16 V, X7R, 0603 | BOOT-SW，最短回路 |  |  |  |  |  |
| P05 | R15_TOP,R15_BOT,C15_FF | 3 | 100 kΩ；22.1 kΩ；47 pF C0G | 3.3 V 反馈和前馈；远离 SW 节点 |  |  |  |  |  |
| P06 | U16 输入/输出旁路 | 6 | 3 × 10 µF 25 V + 3 × 100 nF 25 V | TPS2121 IN1、IN2、OUT 各一组；直接跨相应电源/地 |  |  |  |  |  |
| P07 | C16_SS,R16_ILM,R16_ST | 3 | 100 nF；29.8 kΩ 1%；10 kΩ | TPS2121 约 0.8 V/ms 软启动、典型约 3.5 A 限流；ST 上拉到 3V3 后进 TCA P14（高=电池或无有效输入，低=USB） |  |  |  |  |  |
| P08 | R16_PR_TOP/BOT | 2 | 33 kΩ / 10 kΩ | 电池 PR1；8.4 V -> 1.95 V |  |  |  |  |  |
| P09 | R16_CP_TOP/BOT | 2 | 10 kΩ / 10 kΩ | USB CP2；5 V -> 2.50 V，实现 USB 优先 |  |  |  |  |  |
| P10 | R16_OV1_TOP/BOT | 2 | 75 kΩ / 10 kΩ | TPS2121 IN1 OV ≈ 9.01 V |  |  |  |  |  |
| P11 | R16_OV2_TOP/BOT | 2 | 44.2 kΩ / 10 kΩ | TPS2121 IN2 OV ≈ 5.75 V |  |  |  |  |  |
| P12 | R17_1/2/3 | 3 | 412 kΩ / 25.5 kΩ / 67.3 kΩ | 电池 eFuse UVLO/OVLO 电阻串；结点顺序 VIN-UVLO-OVLO-GND |  |  |  |  |  |
| P13 | R17_ILM,C17_DV,C17_TMR | 3 | 1.33 kΩ；1.2 nF；220 pF | 电池限流约 2.51 A、上电斜率和过流定时 |  |  |  |  |  |
| P14 | R18_OV_TOP/BOT,R18_EN_SER | 3 | 383 kΩ / 100 kΩ；1 kΩ | USB eFuse OVLO ≈ 5.80 V；EN/UVLO 由 U13 的 USB_3A_OK 经 1 kΩ 驱动；不再装 USB UVLO 三电阻串 |  |  |  |  |  |
| P15 | R18_ILM,C18_DV,C18_TMR | 3 | 1.10 kΩ；2.2 nF；220 pF | USB 总限流约 3.03 A、斜率和过流定时 |  |  |  |  |  |
| P16 | eFuse 输入/输出旁路 | 8 | 4 × 1 µF 25 V + 4 × 100 nF 25 V | 两只 eFuse 各自输入/输出；具体最小值按原理图复查数据手册 |  |  |  |  |  |
| P17 | R_FLT_USB,R_FLT_BAT | 2 | 2 × 10 kΩ | U18/U17 的开漏 FLT 分别拉到板载 3V3，进入 TCA P05/P06，保留故障来源 |  |  |  |  |  |
| P18 | U19/U20 旁路 | 8 | 4 × 1 µF 10 V + 4 × 100 nF 10 V | 两只 LM73100 输入/输出各一组；高电流陶瓷回路短 |  |  |  |  |  |
| P19 | C19_DV,C20_DV | 2 | 3.3 nF C0G/X7R | 两路 LM73100 dV/dt；约 10 ms 级母线爬升 |  |  |  |  |  |
| P20 | R20_CMD_SER,R20_CMD_PD | 2 | 365 kΩ；1 MΩ | TCA P13 经 365 kΩ 控制 UBEC LM73100 EN，EN 端 1 MΩ 下拉；上电默认关闭；3.3 V 高态约 2.42 V，按 2.4 V 最低 VOH 和 0.1 µA EN 泄漏仍约 1.73 V，高于 1.223 V 最大开启阈值；较大串阻兼顾误反接钳位电流 |  |  |  |  |  |
| P21 | R19_CMD_PD,R19_CMD_SER | 2 | 100 kΩ；1 kΩ | TCA P07 经 1 kΩ 直接控制 USB LM73100 EN；默认下拉。3 A 硬条件已由上游 USB eFuse 实现 |  |  |  |  |  |
| P22 | C_SER_CER | 4 | 1 µF, 10 V, X7R, 0805 | 与 4 × 220 µF 配对，沿八路舵机接口分布 |  |  |  |  |  |
| P23 | R_BAT_TOP/BOT,C_BAT | 3 | 200 kΩ / 100 kΩ 1%；100 nF | 从 `VBAT_PROT` 取样的电池 ADC 分压和低通；ADC 侧电容紧贴 MCU；禁止改接原始 VBAT，以免 3V3 掉电时反灌 ADC |  |  |  |  |  |
| P24 | R_INA_OUT,C_INA_OUT | 2 | 1 kΩ；100 nF | INA181 到 ADC1 的低通/隔离；约 1.6 kHz |  |  |  |  |  |
| P25 | U19/U20 unused pins | 2 sets | OVLO=GND，IMON=GND，PG/PGTH=NC，DNC=NC | 未使用的 LM73100 模拟脚按数据手册终端处理；IMON/OVLO 不得悬空 |  |  |  |  |  |

## 7. 数字、模拟、音频和外设外围网络

| Item | 参考位号 / 网络 | Qty | 数值 / 器件 | 作用 / 约束 | JLC/LCSC part no. | Price break | Stock / Basic-Extended | Operator proposed substitute | Operator notes |
| --- | --- | ---: | --- | --- | --- | --- | --- | --- | --- |
| S01 | U1 3V3 bypass | 3 | 10 µF + 1 µF + 100 nF | 紧贴 ESP 模组供电脚；额外大容量由 3V3 主干提供 |  |  |  |  |  |
| S02 | R_EN,C_EN,R_BOOT | 3 | 10 kΩ；1 µF；10 kΩ | EN 上拉+RC，GPIO0 上拉；开关分别短接到地 |  |  |  |  |  |
| S03 | R_USB_DP,R_USB_DM | 2 | 22 Ω, 1% | DATA D+/D- 串联预留，靠近 MCU；根据首板稳定性决定 0 Ω 或 22 Ω |  |  |  |  |  |
| S04 | R_CC_DATA1/2 | 2 | 5.1 kΩ, 1% | DATA 口 CC1、CC2 各自 Rd；不得合并 |  |  |  |  |  |
| S05 | DATA_VBUS sense | 4 | 100 kΩ gate 串阻 + 1 MΩ gate 下拉 + 10 kΩ drain 上拉 + 10 nF gate-GND | Q_DATA_SENSE 输出 `DATA_VBUS_N`；插入时为低；DATA VBUS 只检测、不进入任一电源主干 |  |  |  |  |  |
| S06 | U6 bypass | 2 | 1 µF + 100 nF, 10 V | TUSB320 VDD 接 3V3_CC；两个电容靠近器件 |  |  |  |  |  |
| S07 | R_U6_VBUSDET | 1 | 900 kΩ, 1% | 从 USB5_RAW 到 TUSB320 VBUS_DET，按官方典型连接 |  |  |  |  |  |
| S08 | R_U6_OUT1/2 | 2 | 10 kΩ | OUT1/OUT2 开漏上拉至 3V3_CC，只进入 U13 的 3 A NOR 译码 |  |  |  |  |  |
| S09 | R_U6_PORT,R_U6_EN | 2 | 10 kΩ to GND | PORT=UFP；EN_N/LI 上电使能；ADDR 浮空，ID/OUT3 NC |  |  |  |  |  |
| S10 | 3V3_CC supply/status | 8 | U13 100 nF；U14 输入 1 µF/25 V + 100 nF/25 V，输出 2.2 µF/10 V + 100 nF/10 V；Q_USB_OK gate 1 kΩ + 100 kΩ 下拉 + drain 10 kΩ 上拉至板载 3V3 | Type-C 检测域本地旁路和跨电源域状态隔离；TPS709 输出有效电容必须 ≥1.5 µF；U14 EN 直接接 USB5_RAW；所有地共地 |  |  |  |  |  |
| S11 | U2 bypass | 2 | 1 µF + 100 nF | PCA9685 VDD；地址脚 A0-A5 接地 |  |  |  |  |  |
| S12 | R_PCA_OE,R_PWM1-8 | 9 | 10 kΩ 上拉；8 × 100 Ω | OE 默认关闭，由 TCA P15 低态启用；PWM 串阻靠近 U2，降低外线尖峰/振铃 |  |  |  |  |  |
| S13 | R_I2C_SDA/SCL | 2 | 4.7 kΩ | 板上唯一默认装配的 I2C 上拉；总线 3.3 V、短线 |  |  |  |  |  |
| S14 | U3 bypass | 3 | 2.2 µF + 2 × 100 nF | ICM-42670-L 的 VDD/VDDIO；最终按数据手册引脚逐项放置 |  |  |  |  |  |
| S15 | U4 bypass/LR strap | 4 | 1 µF + 100 nF；2 × 0 Ω 互斥焊位（只装一个） | PDM 麦克风本地去耦；L/R 可选接 GND 或 3V3，按固件采样沿实测确认；远离扬声器和 buck SW 节点 |  |  |  |  |  |
| S16 | U5 bypass | 2 | 10 µF + 100 nF | MAX98357A 本地储能；电源和 BTL 输出回路短 |  |  |  |  |  |
| S17 | Amp straps | 3 | GAIN_SLOT 100 kΩ to 3V3；SD_MODE 100 kΩ to GND；TCA P11 到 SD_MODE 串 2 kΩ | 增益约 3 dB；上电默认关断；TCA 输出高启用并选择**左声道**，固件把单声道送 I2S 左 slot；停 BCLK/LRCLK 前先拉低 SD_MODE |  |  |  |  |  |
| S18 | C_SPK_EMI1/2 | 2 | 220 pF, 50 V, C0G，DNP | 配合 R_SPK1/2 磁珠替换位的可选 EMI 滤波；未验证前不装 |  |  |  |  |  |
| S19 | U7 bypass | 1 | 100 nF | INA181 供电旁路，紧贴 VS/GND |  |  |  |  |  |
| S20 | Hall bypass | 4 | 4 × 100 nF | 每只 HAL49ESO 一个；不能只在母线放一个总电容 |  |  |  |  |  |
| S21 | Hall output filters | 8 | 4 × 1 kΩ + 4 × 10 nF | 各 Hall 输出到 ADC1，约 16 kHz；电容靠近 MCU |  |  |  |  |  |
| S22 | U12 bypass,R_INT,R_TCA_RST_PD | 3 | 100 nF；10 kΩ；10 kΩ | TCA9539 去耦、INT 上拉到 3V3；RESET_N 由 GPIO3 驱动且 10 kΩ 下拉；A0/A1 接地，7-bit 地址 0x74 |  |  |  |  |  |
| S23 | Foot input networks | 12 | 4 × 10 kΩ + 4 × 1 kΩ + 4 × 100 nF | 开关到地；约 1 ms 硬件滤波，再做软件去抖 |  |  |  |  |  |
| S24 | LED bypass/data | 6 | 4 × 100 nF + 33 Ω + 100 kΩ | LED DIN 串阻和默认下拉；另加 10 µF 3V3 局部 bulk |  |  |  |  |  |
| S25 | C_LED_BULK | 1 | 10 µF, 10 V, X7R | 四角 WS2812 链局部储能 |  |  |  |  |  |
| S26 | IR TX gate/current | 3 | 33 Ω + 100 kΩ + 18 Ω/0.25 W, 1206 | 3.3 V、LED Vf 典型约 1.4 V 时脉冲约 100 mA；按实购 LED Vf/脉冲规格复核 |  |  |  |  |  |
| S27 | IR receiver supply | 12 | 4 × 100 Ω + 4 × 4.7 µF + 4 × 100 nF | 每个接收器独立 RC 去耦，抑制舵机和 LED 噪声 |  |  |  |  |  |
| S28 | R_IRRX_SER | 4 | 1 kΩ | 每个 IR 接收输出串联后进 ESP RMT/GPIO |  |  |  |  |  |
| S29 | 3V3 generic bypass reserve | 6 | 6 × 100 nF, 10 V, X7R | 原理图完成时按实际位号归并，不得盲删 |  |  |  |  |  |
| S30 | TCA default/unused pulls | 2 | 2 × 100 kΩ | P12 屏幕 BL 和 P17 预留脚各自下拉；TCA 复位时所有端口回到高阻输入，其他安全输出已有各自默认拉阻 |  |  |  |  |  |
| S31 | R_GPIO45_PD,R_GPIO46_PD | 2 | 2 × 10 kΩ to GND | 保证 GPIO45/46 启动绑带为低；GPIO45 启动后可作屏幕输出，GPIO46 启动后才切为 PDM CLK 输出；对应外设不得在复位时强驱高 |  |  |  |  |  |

## 8. GPIO 固化与可引出数量

ESP32-S3-WROOM-1-N16R8 可见 36 个 GPIO，但 Octal PSRAM 占用 GPIO35-37；GPIO0 留作启动恢复，因此有 32 个运行时可分配 GPIO。为证明 5 个剩余脚确实可用，而不只是数量相减，原理图按下表映射；除 GPIO19/20 原生 USB 外，ESP32-S3 GPIO matrix 允许这些数字外设映射。

| GPIO | 固化功能 | 电气/启动约束 |
| --- | --- | --- |
| 0 | BOOT + 恢复测试点 | 10 kΩ 上拉，不算空闲脚 |
| 1、2、10、38 | 空闲/屏幕候选 | 普通输出能力，可作 ST7789 的 SCLK/MOSI/CS/DC/RST 中任意四路 |
| 3 | TCA9539 `RESET_N` / 外设武装 | 10 kΩ 下拉；ESP 启动并完成基础检查后才拉高；ESP 硬件复位时恢复低态，使 TCA 输出回到高阻安全态 |
| 4 | `VBAT_PROT` ADC | ADC1 |
| 5 | INA181 ADC | ADC1 |
| 6、7、8、9 | Hall 1-4 ADC | ADC1 |
| 11、12 | I2C SDA/SCL | PCA9685、IMU、TCA9539 和扩展口共线 |
| 13、15、47、48 | IR RX 1-4 | RMT/输入 |
| 14 | TCA9539 INT | 输入 |
| 16、17、18 | I2S BCLK/LRCLK/DOUT | MAX98357A；SD_MODE 由 TCA P11 控制 |
| 19、20 | USB D-/D+ | 只走 DATA 口，不进入通用排针长支线 |
| 21 | PDM DATA | 普通输入，避免麦克风输出影响启动绑带 |
| 35、36、37 | 不可用 | 被 N16R8 的 Octal PSRAM 占用，禁止连接 |
| 39、40 | CTRL UART TX/RX | 独立于 UART0 恢复测试点 |
| 41 | WS2812 DIN | 四角灯一条串联链 |
| 42 | IR TX | 驱动 AO3400A 栅极 |
| 43、44 | UART0 TX/RX 恢复焊盘 | ROM 下载/日志专用，不接板载外设，避免 USB 失败后无法恢复 |
| 45 | 空闲/屏幕候选，受限 | 启动绑带脚，10 kΩ 下拉；可在启动后作第五路屏幕输出，外部模块复位时不得强驱高 |
| 46 | PDM CLK | 启动绑带脚，10 kΩ 下拉；复位时保持低且无时钟，启动后才切换为 PDM 时钟输出 |

这份映射占用或保留 27 个、留下 GPIO1/2/10/38/45 共 5 个，正好覆盖 ST7789 的 SCLK/MOSI/CS/DC/RST；屏幕 BL 由 TCA P12 低速使能。`J_GPIO_A/B` 必须把五个空闲脚全部引出，并可重复引出 I2C、CTRL UART、3V3、GND 等共享接口，但必须把 GPIO45 标成 `STRAP`、把共享信号标同名，不能把“已引出”误写为“空闲”。GPIO43/44 只能保留恢复焊盘，不能再接 IR；DATA 口 VBUS、功放 SD_MODE、PCA OE、IMU INT 和两路舵机使能由 TCA9539 承担。

## 9. PCB 和原理图的强制实现条件

1. ESP32 天线端伸出板边或按官方 keep-out 禁止铜、器件和线束；buck、电感、扬声器线和舵机母线远离天线与麦克风。
2. USB DATA D+/D- 按 90 Ω 差分目标布线、等长、连续参考地、少过孔；ESD 靠连接器，串阻靠 MCU；USB test pad 不产生支线。
   两个 Type-C 的金属外壳固定脚在连接器附近以多点、低电感方式接板载地；若后续结构引入独立机壳地，连接方式必须重新做 ESD/EMI review。
3. TPS54302 的 VIN-CIN-PGND 和 SW-L-COUT 回路必须按官方推荐布局；反馈从安静的输出点 Kelvin 取样并远离 SW。
4. TPS2121、TPS25947、LM73100 的 Power QFN/HotRod 焊盘和散热铜必须完全遵循各自官方推荐布局。高电流线不能用细颈；5 A 舵机母线的铜厚、外层线宽、温升和连接器焊盘必须用最终层叠重新计算。
5. 5 mΩ 分流器用四线 Kelvin：大电流从端焊盘通过，INA 输入从电阻内侧独立细线对称引出。
6. 6 V UBEC 和 5 V USB 只能在 LM73100 输出端汇合；不得用 0 Ω、跳线或“二选一焊盘”把两源直接并联。
7. TUSB320 的 OUT1/OUT2 真值为：H/H 未连接、H/L Default、L/H 1.5 A、L/L 3 A。只有 U13 检出 L/L，整条 USB eFuse 才会开启；USB 和 UBEC 两只舵机 LM73100 还必须分别收到 TCA P07/P13 的固件命令。GPIO3 低态会硬复位 TCA9539，两路命令另有硬件下拉，因此上电或 ESP 硬件复位时均关闭；Default/1.5 A USB 电源只能给检测域供电，不能启动整板。
   固件必须先验证电源状态、PCA9685 输出和舵机限位，再以 GPIO3 释放 TCA reset；任何主动软重启路径先将 GPIO3 拉低。仅电池供电时，电池 eFuse 掉电会使 TCA 自动回到复位并切断 UBEC；USB 同时维持逻辑时，固件必须根据 `VBAT_PROT` 欠压主动撤销 P13。
8. 舵机电源和逻辑电源共地但分区回流；舵机大电流不得穿过 MCU/IMU/麦克风的地回路。IMU 放在刚性、低振动且远离电感的位置。
9. MAX98357A 是 BTL 输出，SPK+、SPK- 都是开关节点；两线成对、等长、尽量短。底部声孔麦克风与扬声器声腔、舵机结构振动需物理隔离。
10. 所有外接接口在丝印同时标电压和方向；BAT、UBEC、3V3 外设之间采用不同连接器/键位，降低误插风险。
11. 首次上电不接舵机和扬声器，使用限流电源依次验证 VBAT、USB5、mux、3V3、故障阈值和 USB 枚举；然后用电子负载验证，再逐个接入外设。

## 10. 替代件审阅清单

Operator 后续寻找便宜替代时，请保留本表的基准 MPN，在 `Operator proposed substitute` 填完整 MPN，并至少记录：

- JLC/LCSC 编号、单价档位、库存日期、Basic/Extended；
- 官方数据手册链接和制造商；
- 完整后缀、封装尺寸、引脚和推荐焊盘是否相同；
- 输入/输出电压、电流、启动和掉电行为、反向阻断方向、UV/OV/限流误差；
- 对外围阻容、电感、补偿、PCB 铜皮、散热和固件是否有影响。

替代审阅结论统一标记为：**电气/封装等效**、**可用但需改原理图/封装**、**仅可作为实验候选**或**不兼容**。尤其不能用普通 load switch 或肖特基二极管，在未核对反向电流阻断和压降的情况下替代 LM73100；也不能用只支持 5 V 逻辑的 WS2812 版本直接替代 A09。

## 11. 下单前尚未关闭的验证

- 获取所购 ST7789 模块的准确 SKU、引脚、3V3 电流和背光接法，确认 5 个 GPIO 足够。
- 获取电池、UBEC、MG90S 批次和线束，实测八舵机启动/运动/堵转瞬态；必要时提高 UBEC 等级，但不得突破连接器、铜皮和 LM73100 热能力。
- 用准确 HAL49ESO 和 LMD3526B261-OFA03 数据手册复核封装、引脚、回流焊和声孔要求。
- 对 IR 接收器做阳光、LED 灯、舵机噪声和外壳窗口测试；未完成前 A13 保持实验候选。
- 完成原理图后从 CAD 自动导出一次 BOM，与本文件逐位号核对数量；完成 ERC、封装 1:1 校验、3D/结构干涉、电源和 USB review。
- 实体执行器测试必须按仓库安全规则先 mock、后限流台架；确认固定、运动限位、人员净空和急停。

## 12. 计算依据（官方资料入口）

下单和替代审阅以制造商最新官方数据手册为准。当前计算的主要入口如下：

- Espressif：[ESP32-S3-WROOM-1/1U Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)、[ESP32-S3 Hardware Design Guidelines](https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32s3/index.html)；
- TI：[TPS54302](https://www.ti.com/lit/ds/symlink/tps54302.pdf)、[TPS2121](https://www.ti.com/lit/ds/symlink/tps2121.pdf)、[TPS25947](https://www.ti.com/lit/ds/symlink/tps25947.pdf)、[LM73100](https://www.ti.com/lit/ds/symlink/lm7310.pdf)、[TUSB320LI](https://www.ti.com/lit/gpn/TUSB320LI)、[TPS709](https://www.ti.com/lit/ds/symlink/tps709.pdf)、[INA181](https://www.ti.com/lit/ds/symlink/ina181.pdf)、[TCA9539](https://www.ti.com/lit/ds/symlink/tca9539.pdf)；
- NXP：[PCA9685](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf)；TDK：[ICM-42670-P](https://invensense.tdk.com/wp-content/uploads/2021/07/DS-000451-ICM-42670-P-v1.0.pdf)（L 变体仍须用随料文档复核）；
- Analog Devices：[MAX98357A/B](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf)；Worldsemi：[WS2812 family](https://world-semi.com/ws2812-family/)；
- Bourns：[SRP5030TA](https://www.bourns.com/docs/product-datasheets/srp5030ta.pdf)、[MF-PSMF](https://www.bourns.com/docs/product-datasheets/mf-psmf.pdf)；Panasonic：[10SVPE220M](https://industrial.panasonic.com/ww/products/pt/os-con/models/10SVPE220M)；GCT：[USB4105](https://gct.co/connector/usb4105)；Harwin：[M20-7821046](https://www.harwin.com/products/M20-7821046)；C&K：[PTS636SK50JSMTRLFS](https://www.ckswitches.com/products/switches/product-details/Tactile/PTS636/PTS636SK50JSMTRLFS/)；
- 实际采购的 LinkMems、Hallwee、Vishay、Bourns、Panasonic、Littelfuse、连接器和屏幕模块资料。商城参数只能用于交叉检查，不能替代制造商数据手册。
