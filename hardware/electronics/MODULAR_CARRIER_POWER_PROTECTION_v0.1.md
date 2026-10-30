# Modular Carrier fuse and servo-bulk selection v0.1

状态：**Operator-final fuse and optional-bulk decision / prototype validation pending**

日期：2026-09-01

权威输入：[`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md)

## 1. Calculation envelope

The frozen design contract is:

- `EXT_6V_IN = 6.0 V nominal`;
- Operator assumption that purchased PCBA modules, including the PCA9685 Option A power path, have sufficient current capability;
- the single XT30 `J_PWR` feeds `F_MAIN`; its output is protected rail `EXT_6V_IN`;
- `F_MAIN` is frozen at 10 A / ≥32 VDC ATO/ATC;
- all newly designed high-current connector, holder, copper, vias and wire paths provide ≥10 A current-path capability;
- servo bulk is optional prototype tuning, not a condition for correct system operation.

### As-built Gerber note (2026-09-02)

The reviewed prototype Gerber does **not** contain a PCB-mounted ATO/ATC holder. For this prototype, `F_MAIN` is moved into the external power harness, upstream of the board XT30 input. The PCB-holder purchase text in §2 is retained as historical design guidance and is not part of the reviewed Gerber assembly. The external harness fuse remains mandatory before servo bring-up; see [`PCB_GERBER_REVIEW_2026-09-02.md`](PCB_GERBER_REVIEW_2026-09-02.md).

The fuse does not define a precise 10 A load limit. UBEC behavior, servo demand and transient droop remain prototype measurements.

## 2. Main fuse purchase parameter

Frozen selection:

```text
type: replaceable automotive ATO/ATC blade fuse
rating: 10 A
voltage: >=32 VDC
interrupting rating: >=1000 A at 32 VDC preferred
holder: PCB through-hole, >=15 A continuous; 4-terminal holder preferred
quantity: 5 fuses + 2 holders for one prototype/spares
```

Design interpretation:

- Littelfuse ATOF data lists the 10 A element's typical allowed load as 10 A at 20 °C, 8 A at 65 °C and 7 A at 85 °C;
- the same datasheet permits up to 600 s opening time at 135% rating and 0.15–5 s at 200% rating, so this fuse is cable/short protection, not a precise 8 A limiter.

Manufacturer references:

- Littelfuse ATOF 32 V fuse datasheet: <https://www.littelfuse.com/~/media/automotive/datasheets/fuses/passenger-car-and-commercial-vehicle/blade-fuses/littelfuse_atof_datasheet.pdf>
- Littelfuse ATO-FLR PCB holder family: <https://www.littelfuse.com/products/fuse-blocks-fuseholders-and-fuse-accessories/automotive-and-commercial-vehicle-fuse-holders/ato-flr.aspx>

Important coordination limits:

- In UBEC mode, if the UBEC current-limits below 10 A, the fuse may never open; the UBEC's own protection is then the primary short-current limiter.
- In bench-PSU mode, set the PSU current limit to no more than 8 A during bring-up; do not rely on the 10 A fuse for current limiting.
- `J_PWR`, fuse holder, cable and PCB copper must not have a lower safe continuous rating than the intended path.
- A newly selected XT30 footprint or holder guaranteed only to 8 A does not satisfy the frozen ≥10 A current-path requirement.
- The original YSIDO three-pin/Dupont-style connector is removed as the final robot interface. The shortened remaining original lead, heavier-wire transition and XT30 require full-load voltage-drop, temperature-rise and contact-reliability testing.

## 3. Optional servo bulk contract

The Carrier provides two parallel footprints:

```text
references: C_BULK1, C_BULK2
connection: SERVO_6V to GND
capacitance compatibility: 1000–2200 uF each
rated voltage: 16 V
temperature: 105 degC
technology: radial aluminum electrolytic, low impedance/low ESR
body target: diameter <=12.5 mm, lead pitch 5.0 mm
default population: both DNP, or C_BULK1 fitted and C_BULK2 DNP
```

No specific foreign brand, ripple-current threshold or 100 kHz impedance threshold is mandatory. Panasonic `EEUFR1C222` remains a performance and footprint reference only. Reputable domestic parts, including AiSHi series parts, are permitted when backed by a formal datasheet and purchased through a reliable channel.

Placement follows the actual servo-power injection node. With the current Option A architecture, place both footprints near the PCA9685 module V+ input. If power distribution later moves to the Carrier, place them near the Carrier servo-distribution node instead.

Qualification sequence:

```text
0 uF additional bulk
    → full servo test and SERVO_6V capture
    → fit one 1000–2200 uF capacitor if transient droop is excessive
    → repeat capture
    → fit the second capacitor only if evidence requires it
```

These parts improve millisecond-scale events caused by simultaneous servo start, fast reversal, impact and wiring inductance. They cannot compensate sustained stall, inadequate UBEC/battery capability, thin wiring or excessive connector resistance. Consequently 4400 µF is neither a mandatory value nor a mandatory BOM population.

Small Carrier-level decoupling remains permitted where useful:

```text
10 uF / 16 V / X7R / 1206
100 nF / >=16 V / X7R / 0805
```

## 4. Direct-order low-risk passives

For five Carrier boards plus rework stock:

| Value/specification | Package | Buy quantity | Current use |
| --- | --- | ---: | --- |
| 10 kΩ ±1%, ≥0.1 W | 0805 | 100 | PCA `/OE` pull-up, four foot pull-ups, module CS/reset candidates |
| 100 kΩ ±1%, ≥0.1 W | 0805 | 50 | optional default pull-downs |
| 2.2 kΩ ±1%, ≥0.1 W | 0805 | 20 | I²C pull-up candidate; footprint default DNP until module pull-ups measured |
| 4.7 kΩ ±1%, ≥0.1 W | 0805 | 20 | alternate I²C pull-up candidate; do not populate in parallel blindly |
| 33 Ω ±1%, ≥0.1 W | 0805 | 50 | optional SPI/I²S source-series footprints |
| 0 Ω jumper | 0805 | 50 | configurable module links |
| 100 nF, ≥16 V, X7R | 0805 | 100 | foot DNP and Carrier/module local bypass |
| 10 µF, 16 V, X7R | 1206 | 30 | local module/rail bypass |

The fixed 10 kΩ loads dissipate only about `3.3²/10k = 1.09 mW` when asserted, far below a 0.1 W 0805 rating. Yageo `RC0805FR-0710KL / LCSC C84376` is one verified 10 kΩ, ±1%, 125 mW example; equivalent reputable parts are acceptable.

## 5. Evidence still needed

These items are still required before a complete schematic/PCB order, even with the PCBA current-capacity assumption:

1. Exact board-side XT30 male form/MPN, polarity and footprint; source cables use mating XT30 female connectors.
2. Exact ESP32 development board model, top/bottom photos, header spacing, pinout and whether the Carrier feeds `3V3` or a `5V/VIN` pin.
3. Exact PCA9685 module header/pin orientation and `/OE` location; current capacity no longer blocks design by Operator assumption.
4. Exact LM2596S module dimensions, IN/OUT pin order and measured output set to 3.3 V before connection.
5. Exact MPU6500, ST7789, MAX98357A and MS4030 module pinouts; the microphone interface type is especially blocking.
6. YSIDO-to-XT30 cable construction, wire gauge and strain relief.
7. Chosen ATO/ATC holder exact footprint and board keepout.
8. Board outline, mounting-hole positions and module mounting coordinates.

No final fuse/holder, connector or capacitor footprint should be sent to PCB layout until its exact mechanical drawing is attached.

Reproduce the arithmetic with:

```sh
python3 hardware/electronics/calculations/verify_modular_carrier_power_protection.py
```
