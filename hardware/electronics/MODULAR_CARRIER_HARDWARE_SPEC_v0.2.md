# 牛来快跑 Modular Quadruped Controller — Hardware Specification v0.2

状态：**Operator-authoritative architecture / Draft for schematic implementation**

日期：2026-09-01

Owner：Project maintainer

本文件是当前 Carrier PCB 电气工作的唯一权威架构输入，取代归档的 [`MODULAR_CARRIER_HARDWARE_SPEC_v0.1.md`](archive/legacy-integrated-controller/MODULAR_CARRIER_HARDWARE_SPEC_v0.1.md) 双输入/选源方案。旧集成控制板和 v0.1 Carrier 文件只作历史，不得向当前原理图恢复已删除功能。

## 1. Purpose and scope

Rev A 是两层模块化 Carrier / Interconnect PCB，优先两周内实现、首板成功率、低复杂度、易维修，并尽可能使用已购功能模块。Carrier 只负责：

- 单路 6 V 输入、保险、配电和测试；
- LM2596S 模块生成 3.3 V；
- ESP32、PCA9685、MAX98357A、MS4030 等模块互连；
- 八路 MG90S-class servo 供电/信号与四路足端开关；
- 通用 GPIO/电源引出、必要上下拉、去耦和可选 servo bulk。

高度集成 custom controller、custom high-power converter 和市电处理均不在范围内。

## 2. Frozen power architecture

```text
External 6 V source
       │
       ▼
   J_PWR XT30
       │
 F_MAIN 10 A
       │
       ▼
  EXT_6V_IN
       │
       ├── SERVO_6V → servo distribution → Servo ×8
       │       ├── C_BULK1 optional
       │       └── C_BULK2 optional
       └── LM2596S module → 3V3 → logic/modules
```

Carrier 仅有一个主输入 `J_PWR`，且仅允许同时接入一个外部电源。Battery/YSIDO UBEC、bench PSU 和隔离式 220 VAC→6 VDC 商用适配器都在 Carrier 外部转换成统一 XT30。Carrier 不得接触市电。

以下功能明确删除：第二主电源口、DC barrel、screw-terminal 主输入、UBEC/DC selector、ON-OFF-ON switch、source MUX、ideal-diode OR-ing、automatic arbitration 和源并联。

## 3. External source and XT30 policy

- Mobile：Battery → YSIDO UBEC 6 V → 16/18 AWG → XT30。
- Bench：Bench PSU 6 V → banana lead / 16–18 AWG → XT30。
- Wall：commercial isolated 220 VAC→6 VDC adapter → low-voltage cable → XT30。
- Carrier 板端 `J_PWR` 固定使用 XT30 **公头**；所有可能带电的 6 V source cable 使用匹配 XT30 **母头**，减少带电插针裸露和短路风险。
- PCB silkscreen、线束和装配文档必须标识 `+6V`/`GND`。
- 板端公头的 exact MPN 以及直插/卧式/立式等具体形式在 PCB 机械布局时决定；在生成制造封装前必须核对引脚顺序、孔径、外形和插拔 keepout。

YSIDO 原有三针 RC/Dupont-style 接口不作为最终高流接口。在不损害 UBEC 可靠性的前提下，原细线应尽量缩短，转接 16/18 AWG 与 XT30。样机必须测量 XT30、剩余原线和 UBEC 本体的温升与压降。

## 4. Main fuse and high-current class

`F_MAIN` 紧跟 `J_PWR`；两者之间未保护铜皮必须短且宽。保险后电源网命名为 `EXT_6V_IN`。

```text
F_MAIN: ATO/ATC, 10 A nominal, >=32 VDC
holder: PCB THT, >=15 A continuous, multi-pin/four-leg preferred
purchase: approximately 5 fuses and 2 holders
```

Fuse 保护严重短路、PCB、线束、连接器和重大 servo 故障；不是精确 10 A 限流器、servo current regulator 或电子过载控制器。

`J_PWR`、fuse holder、`EXT_6V_IN`、`SERVO_6V`、相关铜皮/过孔/焊盘/焊点均为 **≥10 A design-class path**。这是新设计路径能力，不声明正常负载为 10 A。

## 5. Servo power and optional bulk

`SERVO_6V` 直接分支自受保护的 `EXT_6V_IN`。不设计电子高边切断。

Option A 允许购买的 PCA9685 模块承担 V+ 配电；Option B 是 Carrier 独立分配八路 V+/GND、PCA9685 只承担 PWM 的 fallback。Carrier layout 应尽量保留 Option B 能力。Option A 需检查/实测 connector、copper、temperature、drop 和 V+ geometry。

`C_BULK1`/`C_BULK2` 并联于 `SERVO_6V`–GND：

```text
each compatible: 1000–2200 uF, 16 V, 105 degC
technology: radial low-impedance/low-ESR aluminum electrolytic
preferred body: <=12.5 mm diameter, approximately 5 mm pitch
default: both DNP, or C_BULK1 fitted and C_BULK2 DNP
```

正规国产厂商与可追溯 datasheet 可接受。Bulk 只改善毫秒级加速/换向/线束电感/接触阻抗瞬态，不弥补持续堵转、UBEC/电池功率不足、线径过细或高接触电阻。装配按 `0 uF → C_BULK1 → C_BULK2` 的实测顺序决定，4400 µF 不是强制 BOM。

## 6. Logic power and controller

Purchased LM2596S module converts `EXT_6V_IN` to nominal `3V3`; output must be adjusted and measured before sensitive modules are installed. Generic Carrier parts may include 10 µF/16 V/X7R/1206 and 100 nF/≥16 V/X7R/0805, without duplicating module-internal networks unnecessarily.

ESP32 development board is the central controller; exact board/header/power pins remain TBD. Purchased PCA9685 generates CH0–CH7 servo PWM over I²C. `/OE` uses an external 10 kΩ/0805/1% pull-up to `3V3` so reset/disconnection defaults outputs disabled/Hi-Z. Firmware keeps `/OE` HIGH until I²C, PCA safe values and sensors are initialized, and drives it HIGH on critical motion-control failure. `/OE` is motion disable, not power isolation.

Dedicated electronic ESTOP, back-to-back MOSFET cutoff, fault latch and ESTOP controller remain out of scope. Manual shutdown is removal of the XT30 input.

## 7. GPIO breakout and flying-wire modules

The Carrier shall route every ESP32 GPIO that remains unused after frozen onboard interfaces to accessible 2.54 mm breakout headers/test pads, with adjacent or clearly accessible `3V3` and GND points. Exact header mapping can be frozen only after the exact ESP32 development board is selected.

MPU6500 and ST7789 are connected by Dupont flying wires to this GPIO/power breakout. Therefore:

- no dedicated `J_IMU` or `J_DISPLAY` module footprint, module outline, mounting pattern or point-to-point PCB signal routing is required;
- their logical signal allocation remains documented for firmware and wiring-harness purposes;
- their exact module dimensions do not block Carrier schematic or PCB layout;
- voltage compatibility and actual module pin labels must still be verified before wiring;
- SPI is preferred for both; shared SCLK/MOSI is permitted with independent CS, and MPU6500 may use I²C if later chosen.

This policy does not authorize connecting unknown module pins; it changes only the Carrier implementation from dedicated connector to generic breakout.

## 8. Remaining module interfaces

- MAX98357A + speaker: I²S (`BCLK`, `WS/LRCLK`, `DIN`); exact VIN/SD/GAIN and module header TBD. Speaker output is BTL and neither lead is GND.
- MS4030: actual board must establish analog/amplified/digital interface before GPIO assignment.
- Four active-low foot inputs `FOOT_FL/FR/RL/RR`: 10 kΩ pull-up to 3V3, switch to GND, optional 100 nF DNP to GND, firmware debounce.
- Preferred foot switch `GT-TC164B-H055-L10` remains near-freeze pending mechanical and four-pin continuity verification. Internally common duplicate pins may share their node after proof.
- Floating foot targets approximately 0.5–0.8 mm travel plus a mechanical stop so the switch does not carry structural load.

## 9. Ground, PCB and wiring

All grounds are electrically common, but high-current servo return must not be forced through narrow logic/sensor necks. The two-layer, 1 oz FR-4 board uses short wide pours, top/bottom parallel copper where useful, multiple vias at layer transitions and generous high-current pads. No impedance control, blind/buried vias or HDI are required.

Main source and high-current wiring should use approximately 16/18 AWG where practical. Signal wiring may be smaller. Modules should remain socketed/replaceable using headers, sockets, spacers and screws where practical. Geometry must permit robot-mounted or Ox-Cart-mounted configurations.

## 10. Connectors and test access

Required major interfaces are `J_PWR=XT30`, ESP32, PCA9685, LM2596S, MAX98357A, MS4030, four foot sensors, eight servo channels or Option B fallback distribution, and generic ESP32 GPIO/3V3/GND breakout. MPU6500 and ST7789 use the generic breakout rather than dedicated Carrier connectors.

Mandatory test points are `GND`, `EXT_6V_IN`, `SERVO_6V`, `3V3`, `I2C_SDA`, `I2C_SCL` and `PCA_OE`. Foot inputs may have optional test pads. Test access should remain usable after module installation.

## 11. Still required before final schematic/layout freeze

1. exact ESP32 development board, headers, power pins and exposed GPIO list;
2. complete GPIO allocation and definition of every unused GPIO breakout position;
3. exact board-side XT30 male form/MPN and footprint; source-side female policy and polarity are frozen;
4. exact ATO/ATC holder MPN/footprint/keepout;
5. exact PCA9685 pinout/dimensions and Option A inspection, or selection of Option B;
6. exact LM2596S, MAX98357A and MS4030 module interfaces/dimensions;
7. MPU6500/ST7789 voltage and pin-label verification for the external wiring table only;
8. foot-switch pin pairing and final mechanics;
9. board outline, module coordinates and mounting holes.

## 12. Acceptance tests

- Test YSIDO, bench and isolated adapter sources independently; verify XT30 polarity, 6 V, temperature and wiring temperature.
- Prove `J_PWR → F_MAIN → EXT_6V_IN` continuity and absence of a fuse bypass.
- Verify 3V3 during ESP32 idle, display, audio and servo load; no normal-operation brownout.
- Stage one-to-eight servos and record minimum `SERVO_6V`, UBEC/XT30/remaining original wire/PCB/module temperatures and voltage drop.
- Test servo bulk first at 0 µF, adding `C_BULK1` and then `C_BULK2` only when evidence supports it.
- Verify `/OE` remains HIGH and no uncontrolled pulse/motion occurs during power-up/reset.
- Verify each foot sensor under vertical/angled/repeated touchdown and mild impact, with the hard stop protecting the switch.

Physical actuator testing is HIGH RISK and requires restrained mechanics, current-limited staged bring-up, personnel clearance and immediately accessible XT30 disconnection.

## 13. Frozen/removed status summary

Frozen: modular 2-layer Carrier, one physical board-side XT30 male input with source-side XT30 female cables, physical one-source-at-a-time policy, Battery/YSIDO→XT30, bench→XT30, external isolated 6 V adapter→XT30, 10 A ATO/ATC fuse, ≥15 A holder class, ≥10 A main path, `EXT_6V_IN`, LM2596S→3V3, PCA9685 and `/OE` default disable, optional dual bulk footprints, four foot sensors, and generic unused-GPIO breakout.

Removed: original UBEC Dupont connector as final interface, dual inputs, DC barrel, selector, MUX/OR-ing, direct source paralleling, electronic ESTOP/cutoff, CH32V003, 74HC244, custom 3S→6 V converter, bare PCA9685/MPU6500 and custom MAX98357A circuit.

TBD: exact ESP32, GPIO allocation/breakout map, servo power-distribution Option A/B, exact module/connector footprints, final foot mechanics and PCB geometry.

## 14. Design baseline

The Carrier uses one standardized 6 V XT30 input and no source-selection circuitry. `F_MAIN` immediately protects the input, and `EXT_6V_IN` feeds `SERVO_6V` and the LM2596S 3.3 V module. PCA9685 drives eight servos with hardware-default `/OE` disable. Two optional 1000–2200 µF / 16 V / 105 °C servo-bulk positions are test-defined. MPU6500 and ST7789 use Dupont flying wires through generic ESP32 GPIO/power breakout, while all other unused ESP32 GPIOs are also exposed.

## 15. Reviewed prototype Gerber status

The 2026-09-02 prototype Gerber review is recorded in [`PCB_GERBER_REVIEW_2026-09-02.md`](PCB_GERBER_REVIEW_2026-09-02.md). The reviewed board is **conditionally approved for prototype fabrication**. In this as-built prototype, `F_MAIN` is implemented in the external power harness before the board XT30 rather than as a PCB component. The external 10 A ATO/ATC fuse is mandatory before energizing the board, while the board-side 10 kΩ OE pull-up must be populated manually between the two silkscreened through-holes.

The review also accepts, for this prototype only, the 12 isolated copper annuli around six NPTH mounting holes and the absence of onboard bulk-capacitor footprints. These are known deviations from the clean-layout preference, not evidence of an electrical short or an assembled component. The NPTH/antenna-area note and all remaining qualification limits are maintained in the review record.
