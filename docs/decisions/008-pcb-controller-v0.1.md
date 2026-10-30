# Decision: Modular Carrier PCB for the 牛来快跑 prototype

Status:
- Proposed

Problem:
The two-week prototype needs reliable interconnection and eight-servo power distribution without taking on the schedule and assembly risk of a dense integrated controller PCBA.

Options:
- Continue loose breadboard/module wiring.
- Build a 2-layer modular Carrier using purchased functional modules.
- Build the earlier four-layer integrated ESP32/power/sensor/audio controller.
- Split power and logic across multiple custom boards.

Current hypothesis:
Use the 2-layer modular Carrier defined by [`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](../../hardware/electronics/MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md). Battery/YSIDO, bench and isolated wall-adapter sources are converted externally to mutually compatible 6 V XT30 cables; only one is physically connected. The single `J_PWR` feeds a replaceable 10 A / ≥32 VDC ATO/ATC fuse; protected `EXT_6V_IN` feeds the PCA9685 module/servo rail and purchased LM2596S 3.3 V module. There is no second input, barrel jack, selector or source-arbitration circuit.

ESP32, PCA9685, MPU6500, ST7789, MAX98357A, MS4030 and LM2596S remain replaceable modules. PCA9685 `/OE` has a 10 kΩ pull-up so PWM is disabled during ESP32 reset. The Carrier does not include CH32V003/74HC244, automatic source muxing, electronic ESTOP, electronic servo cutoff or custom high-density power/audio/sensor circuits.

Operator selections recorded 2026-09-01:

- PCA9685 power-distribution Option A is selected conditionally: the purchased module carries servo V+ only after physical copper/connector/drop/temperature verification.
- MPU6500 and ST7789 use Dupont flying wires from generic ESP32 GPIO/3V3/GND breakout; they do not receive dedicated Carrier footprints or PCB signal routing.
- Every ESP32 GPIO unused after final allocation is exposed on accessible 2.54 mm breakout headers/test pads.
- Carrier `J_PWR` is an XT30 male and energized source cables use mating XT30 females; exact board-mount male form/MPN is deferred to PCB mechanical layout.

Operator protection decision recorded 2026-09-01:

- New main-current paths are designed for ≥10 A capability; the ATO/ATC PCB holder is rated ≥15 A continuous and preferably uses multiple solder terminals.
- `C_BULK1` and `C_BULK2` are optional 1000–2200 µF / 16 V / 105 °C radial low-impedance footprints. Default population is both DNP or one fitted; measured `SERVO_6V` transients decide the final BOM.

Experiment:

1. Identify every purchased module and record its pinout, dimensions, voltage limits and onboard pull-ups/regulators.
2. Verify every source cable's XT30 polarity and enforce one-source-at-a-time physical connection.
3. Characterize the PCA9685 module V+ path before connecting more than one restrained servo.
4. Set and verify LM2596S output before installing 3.3 V modules.
5. Confirm `/OE` remains HIGH through boot/reset and no valid servo pulse reaches CH0–CH7 before explicit arm.
6. Stage one-to-eight-servo load while recording rail minima, connector/module/PCB temperatures and ESP32 brownouts.

Result:
Review Candidate 4 的 Carrier schematic 和 2026-09-02 prototype Gerber 已存在，PCB 也已生产。Gerber 电气复审为有条件通过，Operator 在生产后接受制造放行 `PASS`；完整的实物电源、装配和机械 qualification 仍未完成。机器可检查的接口表仍是候选分配，并明确保留未解决的模块/连接器证据。

Decision:
The requesting Operator replaced the earlier integrated-controller direction with the modular Carrier architecture on 2026-08-31. The architecture is the active design baseline but remains Proposed pending exact-module interface freeze, schematic/ERC, physical power-path evidence and non-author human review.

Consequences:
- The earlier v0.1–v0.8 integrated-controller documents remain design history only.
- A two-layer, through-hole/header-oriented Carrier is allowed; complex custom QFN/WLP assembly is outside this revision.
- The high-current route through XT30, fuse, PCA9685 module and servo connectors is HIGH RISK and requires measured qualification; a 10 A fuse is not a precise current limiter.
- `/OE` is motion disable, not a power disconnect; manual OFF/disconnection must remain immediately accessible during restrained testing.
- Firmware still uses hardware-independent adapters and gains no permission for gait planning, automatic leg selection, compensating steps or whole-body balance.
