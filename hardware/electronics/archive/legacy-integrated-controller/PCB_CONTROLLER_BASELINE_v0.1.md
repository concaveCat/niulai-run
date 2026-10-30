# 牛来快跑 PCB Controller Baseline v0.1

Status: **Historical Proposed Baseline / superseded as design input**

Owner: Project maintainer

Prepared: 2026-08-29

Current accepted design input: [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md); this baseline is superseded.

Historical calculated BOM: [`PCB_CONTROLLER_BOM_v0.1.md`](PCB_CONTROLLER_BOM_v0.1.md)

## 1. Scope and authority

This document records the earlier 2S/5 A **Proposed Engineering Baseline**. It was superseded as a design input on 2026-08-30 by the accepted 3S/6 A freeze record linked above. Its battery thresholds, external Y-cable topology, servo power path, UART/IR scope and GPIO budget must not be copied into the new schematic. It does not report a completed electrical design or bench result.

The baseline must not add gait planning, automatic leg selection, automatic recovery steps, or unapproved whole-body balance. Sensors may report events, record data, score gameplay, or trigger hardware protection without taking control timing away from players.

## 2. Acceptance criteria for the next schematic stage

The Rev A schematic is ready for review only when:

1. every required block below has an assigned part, rail, reference designator, connector, and protection path;
2. battery, USB POWER, USB DATA, 3V3, and servo returns cannot back-feed one another outside the documented path;
3. every ESP32-S3 pin has one authoritative assignment or is labelled as a free/header GPIO, including boot-strapping and N16R8 PSRAM restrictions;
4. power estimates include normal, startup, and fault current rather than only nominal current;
5. the exact `HAL49ESO`, IR receiver, connector, regulator, eFuse, and source-selection parts have verified datasheets and footprints;
6. schematic ERC, pin/footprint review, and an independent human review are recorded before PCB layout.

## 3. Frozen functional architecture

### 3.1 Controller and communications

- MCU/module: `ESP32-S3-WROOM-1-N16R8`.
- Wireless player input: ESP32-S3 internal BLE; no external Bluetooth module.
- Computer port: one dedicated USB-C DATA connector using ESP32-S3 native USB Serial/JTAG on GPIO19/20.
- USB DATA functions: firmware flashing, bidirectional virtual serial console, and built-in JTAG.
- Recovery: physical BOOT and RESET controls plus GPIO0, EN, UART0 TX, and UART0 RX test points.
- Control UART: one separate 3.3 V TTL header, nominally `3V3/GND/CTRL_TX/CTRL_RX`.
- The four player buttons are remote controller data received over BLE or the control UART. They do not consume four local button GPIOs.

USB CDC serial is a computer-visible serial channel but is not the same electrical interface as the 3.3 V control UART.

### 3.2 Motion and contact

- Eight servo outputs controlled by one `PCA9685`-class 16-channel PWM controller.
- Eight three-pin servo connectors: `SERVO_V+`, `GND`, and PWM.
- A hardware servo-output enable/shutdown path must place the actuators in a safe disabled state during reset and fault handling.
- Four foot-contact inputs for the planned KW10 switches, with replaceable/debounced sensor interfaces.
- Remaining PCA9685 channels are not assigned to visible LEDs because all PCA9685 channels share the servo PWM frequency.

### 3.3 Sensors

- IMU: `ICM-42670-L`, powered from 3V3, I2C plus at least INT1. INT2 is routed when pin budget permits or retained as a test pad.
- Microphone: `LMD3526B261-OFA03`, 3V3 PDM interface to ESP32-S3 I2S0.
- Servo-rail current sensing: `INA181A2IDBVR`, 3V3 supply, high-side Kelvin measurement, 5 mOhm shunt, analog output to an ADC1-capable GPIO.
- Magnetic gameplay sensing: four `HAL49ESO` candidate linear Hall sensors on 3V3 and four ADC1-capable inputs. Exact manufacturer, pinout, transfer curve, and package remain procurement blockers.
- Battery voltage: divided and filtered into an ADC1-capable input; divider must not exceed ADC limits at 8.4 V and must not create unacceptable off-state battery drain.

The four Hall sensors are intended for underside magnetic checkpoint patterns, polarity/strength detection, coarse left/right checkpoint position, magnetic game items, or dock/cover detection. They are not an absolute-position system or a compass.

### 3.4 Audio and visible feedback

- Amplifier: `MAX98357AEWL+T`, 9-bump WLP, digital I2S input, default 3V3 supply.
- Speaker: one off-board 3516-format, 4 ohm, 2 W speaker.
- Speaker connection: keyed/locking two-pin connector, nominally JST-PH 2.0 mm. Both pins are BTL outputs (`SPK+` and `SPK-`); neither is ground.
- Four corner LEDs: `WS2812C-2020` preferred, or an exact 3.3 V-capable `WS2812B-2020-V6` substitute. They form one daisy chain and use one ESP32 data GPIO.
- LED brightness is software-limited. Each LED receives local decoupling; the chain receives a series data resistor and local bulk capacitance.
- No LED, copper, or trace may violate the ESP32 module antenna keep-out. A mechanically corner-mounted light may require an inward LED and a light pipe.

`MAX98357AEWL+T` is accepted only if the chosen assembler supports 0.4 mm-pitch WLP without a surcharge that removes its cost advantage. The layout must use the manufacturer's land pattern and the first build should receive appropriate assembly inspection.

### 3.5 Infrared gameplay

- ESP32-S3 RMT generates and decodes the modulated protocol; no dedicated protocol IC is required.
- One 940 nm IR transmit channel uses an external transistor/MOSFET and current-limited emitter.
- Four independently readable 38 kHz receiver positions are planned for front/rear/left/right hit direction.
- Receiver heads may be fitted to the main board edge or connected as small outward-facing modules; optical line of sight takes priority over shortest wiring.
- Rev A protocol fields should include sender ID, message type, sequence number, and checksum/CRC.
- A minimum-cost assembly option may populate only one receiver, losing direction information but retaining attack/checkpoint functionality.

### 3.6 Display and expansion

- Display: off-board packaged 170 x 320 ST7789 module connected through general GPIO headers; no dedicated display connector or permanently reserved display pins.
- Expose all genuinely usable free GPIOs after accounting for N16R8 octal PSRAM, native USB, boot-strapping, and on-board functions.
- Headers must distinguish free GPIOs from shared/isolatable signals.
- Provide repeated 3V3 and GND pins alongside GPIOs.
- Keep servo power on a separate keyed connector family; do not place 5/6 V beside 3V3 GPIO power in a confusable header.
- Reserve one 3V3 I2C expansion port (`3V3/GND/SDA/SCL/INT`) and two generic sensor ports.

## 4. Frozen power intent

### 4.1 Rails

| Rail | Source | Loads | Notes |
| --- | --- | --- | --- |
| `VBAT` | 2S battery, approximately 6.0-8.4 V | logic buck input, external 6 V UBEC input, battery monitor | Board does not charge or balance the 2S pack |
| `5V_USB_PWR` | dedicated USB-C POWER source, 5 V/3 A | selectable servo source, logic power input | Must not be assumed to be 3 A until CC advertisement is detected |
| `3V3` | `TPS54302` reference design from selected battery/USB input | ESP32, sensors, audio, LEDs, logic and headers | 3 A target with calculated transient margin; effective capacitance, copper and temperature still require PCB verification |
| `SERVO_V+` | external 6 V/5 A UBEC or protected USB 5 V path | eight servos only | The 6 V and 5 V sources must never be hard-paralleled |

### 4.2 USB ports

USB-C POWER:

- dedicated 5 V input, nominally 3 A;
- `TUSB320LIRWBR` detects attach and advertised default/1.5 A/3 A current mode;
- a pre-eFuse, wide-input `TPS70933` detection domain and `SN74LVC1G02` decode the 3 A state; only that state enables the `TPS259470L` USB eFuse;
- the eFuse performs current limiting, short-circuit and reverse-current protection;
- TUSB320 is not a USB-PD controller and does not regulate the load current;
- USB-powered servo operation is a reduced-current bench mode. Eight MG90S servos must not be assumed to move or stall simultaneously within 3 A.

USB-C DATA:

- CC1 and CC2 each use the required sink termination;
- D- routes to GPIO19 and D+ to GPIO20 with USB ESD protection and controlled differential routing;
- VBUS is detected/protected for a self-powered board but is not a servo or system-power source;
- data-port VBUS must not back-feed the computer or the POWER connector.

### 4.3 Battery operation

- The 2S battery powers both board logic/peripherals and the external UBEC servo path.
- An external balance charger remains mandatory; no onboard 2S charger is included.
- Battery input requires a fuse, reverse-polarity protection, voltage monitoring, and an explicit low-voltage policy.
- Logic source priority and servo source selection must be unambiguous when battery and USB POWER are both connected.

The calculated Rev A reference uses separate `LM73100` true-reverse-current-blocking paths for USB 5 V and UBEC 6 V, followed by one common shunt and servo rail. Both paths are hardware-default-off and require independent TCA9539 firmware enables; USB enable additionally depends on the upstream hardware 3 A qualification. GPIO3 holds the TCA9539 in hardware reset until firmware explicitly arms the peripheral domain. This remains Proposed until schematic review and source-transition bench testing.

## 5. ESP32 direct capabilities used by this board

| ESP32-S3 peripheral | Board use |
| --- | --- |
| BLE | player controller reception |
| USB Serial/JTAG | flashing, virtual serial console, JTAG |
| hardware UART | separate 3.3 V control UART |
| I2C | PCA9685, ICM-42670-L, optional expansion |
| I2S standard TX | MAX98357A digital audio |
| I2S0 PDM RX | digital microphone |
| RMT TX/RX | IR carrier/protocol; optionally addressable LEDs |
| ADC1 | current, battery, and four Hall channels |
| GPIO matrix | external ST7789 and exposed expansion pins |

RMT provides timing and coding but cannot drive a high-current IR emitter or sense infrared light without external optical components. ADC measurements require board calibration and filtering; they are not automatically precision instruments.

## 6. Intended gameplay capabilities

This board can support player-timed four-leg control, contact-aware leg states, tilt/fall logging, audio input/output, screen feedback, four-corner player/leg lighting, energy/effort scoring from servo current, magnetic checkpoint tiles, IR attack/hit/checkpoint messages, BLE or UART controllers, and PC logging/debugging.

Optional sensing must remain an adapter/capability. It must not silently introduce gait planning, automatic leg choice, automatic compensating steps, or full-body closed-loop balance.

## 7. Open items before schematic freeze

1. Schematic/ERC and thermal validation of the calculated `TPS54302`、`TPS2121`、`TPS259470L` and dual-`LM73100` power path, including source transitions and fault reset behavior.
2. Marketplace confirmation or reviewed substitutes for the USB-C, battery, UBEC, servo, foot, UART, GPIO, Hall/IR-module, and speaker connector SKUs and orientations.
3. Exact manufacturer datasheet and verified pinout for `HAL49ESO`.
4. Exact inexpensive 38 kHz IR receiver and 940 nm emitter, including 3V3 compatibility and optical/mechanical placement.
5. Whether all four IR receivers are populated or only their interfaces/footprints.
6. Exact ST7789 module pinout, logic voltage, current, connector, and mechanical envelope.
7. Map the calculated functional GPIO budget to exact ESP32 pin numbers and confirm all selected ADC inputs are ADC1-capable and do not conflict with boot or USB.
8. Validate the calculated 3V3/USB budgets against exact display and servo measurements, then check trace width, connector current rating, thermal rise, and brownout behavior.
9. PCB dimensions, mounting holes, antenna clearance, microphone acoustic port, LED light pipes, and outward-facing IR geometry.
10. Assembly quotation and process confirmation for `MAX98357AEWL+T` WLP.

## 8. Minimum verification plan

Before ordering Rev A:

- run schematic ERC and a manual rail/back-feed review;
- review every symbol against the exact manufacturer datasheet and package drawing;
- check the BOM against JLC/LCSC availability and assembly class without silently substituting parts;
- calculate worst-case 3V3 and USB current and temperature rise;
- inspect antenna, WLP, microphone-port, Kelvin-shunt, USB differential-pair, IMU, Hall, and high-current layout constraints;
- obtain a non-author human review.

After assembly, power logic from a current-limited supply before fitting servos; then validate USB flashing/console, BLE/UART input timeout, all ADC baselines, IMU identity/orientation, microphone noise, speaker power/temperature, LED current, IR range/crosstalk, battery cutoff, source switching, and servo-rail transients. Motion tests require mechanical restraint, limits, personnel clearance, and an accessible emergency stop.
