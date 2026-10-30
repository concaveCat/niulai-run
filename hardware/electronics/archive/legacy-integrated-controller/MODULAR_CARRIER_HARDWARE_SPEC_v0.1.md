# 牛来快跑 Modular Quadruped Controller — Hardware Specification v0.1

状态：**Superseded by Hardware Specification v0.2; retained as dual-input design history**

取代文件：[`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](../../MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md)

日期：2026-09-01

Owner：Project maintainer

本文件是当前 Carrier PCB 电气工作的唯一权威架构输入。它取代 `PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.1` 至 `v0.8` 的活跃权威地位；旧文件只保留为集成控制板方案的设计历史。未在本文重新保留的旧功能、器件和约束不得进入当前原理图。

## 1. 目标与范围

Rev A 使用商用功能模块，Carrier PCB 主要负责：

- 6 V 电源输入、人工选源、保护和分配；
- 模块互连、安装、连接器和测试点；
- 八路 MG90S-class 舵机供电与信号连接；
- 四路足端开关；
- 必要 pull-up/pull-down、Carrier-level 去耦和大电容预留。

设计优先级依次为：约两周内实现、首板成功率、易调试维修、低制造复杂度、满足八舵机原型的电气鲁棒性。高度集成 custom PCBA 不属于本 revision。

## 2. 冻结系统架构

```text
Battery → YSIDO UBEC 6 V → J_UBEC ─┐
                                    ├→ SW_PWR (UBEC/OFF/DC, break-before-make)
Bench PSU 6 V → J_DC barrel ───────┘
                                    ↓
                                6V_UNFUSED
                                    ↓
                              F_MAIN 10 A
                                    ↓
                                EXT_6V_IN
                                    ↓
                         ┌──────────┴──────────┐
                         │                     │
                     SERVO_6V           LM2596S module
                         │                     │
                  PCA9685 module V+           3V3
                         │                     │
                    eight servos      ESP32/PCA logic/modules
```

- `J_UBEC` 与 `J_DC` 是两个独立物理入口；正极不得直接并联。
- `SW_PWR` 是 `ON-OFF-ON`、break-before-make，切换正极；两路 GND 保持共地。
- `6V_UNFUSED` 为选源后、保险前的 6.0 V nominal 网络；除保险丝输入和测试点外不得向负载分支。
- `EXT_6V_IN` 为 `F_MAIN` 之后的受保护 6 V 主轨。
- `F_MAIN` 固定为 10 A、≥32 VDC ATO/ATC 汽车片式保险丝，保护 PCB、线束、连接器和严重舵机故障；它不是精确限流器、Servo 电流控制器或 ESTOP。
- 已购 LM2596S 6 V→3.3 V / 3 A 模块提供逻辑电源；Carrier 不设计 custom 3V3 buck。
- 采用 PCA9685 商用模块产生 8 路 PWM；CH32V003、74HC244 和裸 PCA9685 均从当前范围删除。
- 不设计 electronic ESTOP、电子舵机高边切断、fault latch 或 ARM MOSFET；人工断电依靠 `SW_PWR` 中位 OFF 或拔除主电源。
- PCA9685 `/OE` 是正常电子 motion-disable，硬件默认 HIGH/disabled。

## 3. 电源输入与人工选源

### 3.1 `J_UBEC`

- 使用现有 YSIDO UBEC 的 3-position RC / 2.54 mm-class connector。
- 放板前必须用实物确认 `+6V`、`GND` 和第三触点；第三触点在确认前定义为 `NC/TBD`。
- Carrier 不把第三触点猜测为 signal。

### 3.2 `J_DC`

- Operator 选择 DC barrel jack 作为台式 6 V 入口。
- exact jack/plug/cable 三者必须作为一套确认中心正极、内外径、额定 DC 电流和温升。
- “供电线满足电流”不能单独证明 jack 的弹片、焊脚和 PCB footprint 满足电流。
- exact MPN 未选前保留 `TBD`；不得从“DC5.5”名称推断 2.1/2.5 mm 内径。

### 3.3 `SW_PWR`

- `ON-OFF-ON`，break-before-make，positive-conductor switching。
- 位置为 `UBEC / OFF / DC`；转换过程中不得短时并联两源。
- 新设计的主电流路径按 ≥10 A current-path capability 设计；必须核对开关的 **DC** breaking/carrying rating，而不是只引用 AC rating。
- exact MPN、端子方向、机械操作空间和 PCB/线束实现均为 layout 前 blocker。

## 4. Servo power 与 Option A

Operator 选择 Option A，并于 2026-08-31 明确要求电路设计阶段假设所购 PCBA 模组的电流能力完全足够：

```text
SERVO_6V → purchased PCA9685 module V+ path → module CH0..CH7 servo headers
```

该选择是 **Accepted system assumption / prototype verification pending**。以下项目不再阻塞 Carrier schematic/layout，但仍必须在样机逐级上电时检查并记录：

- PCA9685 模块 exact board/version、V+ 入口和 servo header pin order；
- V+ 与 GND 的最窄铜宽、层数/铜厚、过孔和连接器额定；
- 8 A 级短时压降和温升；
- 模块 bulk capacitor 的容量、耐压和连接位置；
- `/OE=HIGH` 时输出确为 Hi-Z，但 servo V+ 仍带电这一边界。

若样机验证出现不可接受压降或温升，则回到 Option B，由 Carrier/PDB 独立分配 `SERVO_6V`，PCA9685 只承载 PWM。当前文件把“足够”记录为 Operator 接受的假设，不把它改写为 manufacturer guarantee。

`F_MAIN` 额定值已冻结为 10 A。后续实测仍用于确认 UBEC 限流行为、连接器压降/温升和保险丝与主路径的配合，不把 10 A 保险丝解释为 10 A 精确限流。

## 5. 逻辑与模块

| Function | Frozen architecture | Detail before schematic freeze |
| --- | --- | --- |
| main controller | ESP32 development board | exact board model/header spacing/pinout/power pin |
| PWM | purchased PCA9685 module, CH0–CH7 | exact module dimensions/pins/address jumpers/onboard pull-ups |
| logic power | purchased LM2596S module set to 3.3 V | exact module pinout, dimensions, adjustment lock and load test |
| IMU | purchased MPU6500 module | exact pinout; SPI preferred, I2C allowed |
| display | purchased ST7789 module | exact pinout; SPI preferred; VCC/BL behavior |
| audio out | purchased MAX98357A module + speaker | exact VIN range, SD/GAIN/header pinout; I2S preferred |
| microphone | purchased MS4030 module | analog/amplified/digital interface must be identified |
| foot contact | 4 × binary switch, active-low | preferred `GT-TC164B-H055-L10`, physical fit pending |

Module carrier pins may receive 100 nF/10 µF local bypass where useful, but internal module networks are not duplicated without evidence. `SERVO_6V` reserves two parallel radial aluminum-electrolytic footprints, `C_BULK1` and `C_BULK2`, each compatible with 1000–2200 µF / 16 V / 105 °C low-ESR/low-impedance parts, diameter ≤12.5 mm and 5 mm lead pitch. Both are optional tuning parts: first-build default is both DNP or only `C_BULK1` populated; measured servo transients determine final population. Panasonic `EEU-FR1C222` is only a performance reference, and reputable domestic capacitors with formal datasheets are permitted.

## 6. PCA9685 safe default

```text
3V3 ── 10 kΩ ── PCA_OE
                   └── ESP32 GPIO
```

- `/OE=HIGH` disables PCA9685 outputs; `/OE=LOW` enables outputs.
- ESP32 reset/high-impedance or disconnected control defaults `/OE` HIGH.
- Firmware keeps `/OE` HIGH through boot, I2C setup, PCA initialization and safe-channel writes; only system-ready state may drive it LOW.
- A critical software/control fault drives `/OE` HIGH.
- This is motion disable, not power isolation; it does not remove `SERVO_6V`.

## 7. Foot inputs

Each `FOOT_FL/FR/RL/RR` uses:

```text
3V3 ── 10 kΩ ── FOOT_x ── external switch ── GND
                         └─ optional 100 nF DNP to GND
```

Open/not pressed is HIGH; pressed is LOW. Firmware provides debounce. Mechanical design targets approximately 0.5–0.8 mm compliant travel and a hard stop so the switch does not carry structural load.

## 8. Ground and layout constraints

All source, servo, logic and module grounds are electrically common. Carrier layout must nevertheless keep high-current servo return paths short and wide and prevent them from intentionally traversing narrow sensor/logic ground necks.

Baseline board construction is 2-layer FR-4, nominal 1 oz, no controlled impedance, no blind/buried vias and no HDI. High-current paths use short top/bottom pours and multiple vias where layer transfer is unavoidable. Complex QFN/WLP assembly is not required because active functions are modules.

The newly designed route from `SW_PWR` through `F_MAIN`, `EXT_6V_IN`, `SERVO_6V` and the servo-power injection node must provide ≥10 A current-path capability. This requirement also applies to `J_DC`, the selector, fuse holder, associated pads and layer-transfer vias. The existing YSIDO three-pin connector is a module constraint rather than a claimed 10 A-rated new design; full-load qualification must measure its voltage drop, temperature rise and contact reliability. An unacceptable result requires a higher-current interface such as XT30.

`C_BULK1` and `C_BULK2` connect from `SERVO_6V` to GND and must be placed near the actual servo-power distribution/injection node. Under Option A this means near the PCA9685 module V+ input. They buffer millisecond-scale wiring/UBEC transients only and cannot correct sustained servo stall, inadequate source power, thin wiring or high connector resistance.

## 9. Required connectors and test points

Required interfaces: `J_UBEC`, `J_DC`, `SW_PWR`, PCA/servo CH0–CH7, four foot inputs, ESP32, PCA9685, MPU6500, ST7789, MS4030, MAX98357A and LM2596S module connections.

Mandatory test points: `GND`, `6V_UNFUSED`, `EXT_6V_IN`, `SERVO_6V`, `3V3`, `I2C_SDA`, `I2C_SCL`, `PCA_OE`. Foot and selected audio/display signals may receive optional pads.

## 10. Current freeze and blockers

Frozen: modular Carrier, 2-layer board, Battery→YSIDO 6 V, dual connectors, manual `UBEC/OFF/DC` selection, no source paralleling, `6V_UNFUSED → F_MAIN 10 A → EXT_6V_IN`, new main-current paths rated ≥10 A, optional dual servo-bulk footprints with DNP-first tuning strategy, LM2596S module, ESP32 architecture, PCA9685 module, `/OE` default-disable, Option A intent, MPU6500/ST7789/MAX98357A/MS4030 modules and four foot sensors.

Still blocking schematic or layout freeze where applicable:

1. exact ESP32 development board and header pinout;
2. YSIDO three-pin polarity;
3. PCA9685 module pinout/dimensions；Option A 电流能力按 Operator 假设推进，实测转为 PVR；
4. MPU6500/ST7789/MS4030/MAX98357A/LM2596S exact module pinouts and dimensions;
5. exact DC barrel jack/plug/cable pair and DC current rating;
6. exact break-before-make selector and DC rating;
7. exact ATO/ATC PCB holder MPN/footprint; fuse type and 10 A/≥32 VDC rating are frozen;
8. GPIO allocation after exact ESP32 board exposure is known;
9. servo connector orientation, exact compatible bulk capacitor body height/footprint variants, board dimensions and mounting holes.

The former v0.1 interface table has been superseded by the active [`MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv`](../../MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv), which follows Hardware Specification v0.2.

## 11. Preliminary acceptance sequence

1. With servos disconnected, verify each selector position and absence of reverse power between input ports.
2. Verify LM2596S output before installing logic modules; then test ESP32 boot/display/audio load without servos.
3. Hold PCA `/OE` HIGH and inspect all CH0–CH7 signals during power-up/reset.
4. Connect one restrained servo, then stage additional servos while measuring `SERVO_6V`, connector/module temperature and 3V3 brownout.
5. Test additional servo bulk in the sequence `0 µF → one 1000–2200 µF part → second part only if needed`, recording `SERVO_6V` transient minima for each configuration.
6. Run full module set only after the source selector, fuse, PCA V+ path and wiring pass their measured current/temperature limits.

Any test that changes motion, power or protection behavior is HIGH RISK: fixed mechanics, current-limited source, personnel clearance and immediately accessible manual OFF/disconnect are required.
