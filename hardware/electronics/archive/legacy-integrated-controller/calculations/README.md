# PCB Controller Rev A — reproducible circuit-design checks

These files support paper circuit design only. Passing a script is not schematic ERC, PCB review, or prototype validation.

## External battery-front-end draft validation

[`GLM_BATTERY_FRONTEND_VALIDATION_v0.1.md`](GLM_BATTERY_FRONTEND_VALIDATION_v0.1.md)
independently reviews the repository-root GLM draft. Run:

    python3 hardware/electronics/calculations/validate_glm_battery_frontend_draft.py

The review accepts only reproducible arithmetic, records seven source/interpretation
defects, and supplies corrected first-pass RSENSE, PWR/TIMER, MOSFET conduction and
fuse values. It does not accept the candidate topology.

The external model's revised calculation is reviewed in
[`GLM_BATTERY_FRONTEND_VALIDATION_v0.2.md`](GLM_BATTERY_FRONTEND_VALIDATION_v0.2.md).
It is accepted as a corrected calculation draft, but the PWR/TIMER/SOA,
reverse-current, TVS, OVLO and fault-policy engineering conclusions remain open.

## SY8388ARHC buck first-pass screen

Run:

    python3 hardware/electronics/calculations/verify_sy8388_buck.py

This sizes the 6.0 V feedback divider and screens duty cycle, inductor ripple/
peak/RMS current, input-capacitor RMS current, internal MOS conduction loss,
evaluation-board thermal power and a first-pass effective output-capacitance lower
bound. It intentionally does not select exact passive MPNs or claim prototype/EMI
closure.

The current selected electrical targets are 2.7 µH ±20%, at least 40 µF
effective input capacitance with a combined 4 A RMS rating, and at least 330 µF
effective output capacitance with no more than 10 mΩ combined ESR. Exact passive
MPNs and total loss remain open.

## ESTOP passive front end

Run:

    python3 hardware/electronics/calculations/verify_estop_frontend.py

This checks the 1 kΩ/10 kΩ/100 nF normally-closed loop filter over resistor and
capacitor tolerance against conservative 0.3/0.7 VDD CMOS thresholds. It does
not select the Schmitt gate, latch, POR or ESD MPN.

## CH32 eight-channel PWM contract

Run:

    python3 hardware/electronics/calculations/verify_ch32_pwm_protocol.py

This checks the fixed 0x40 I2C address, 48 MHz-to-1 MHz timer division, 50 Hz
frame, 500–2500 µs pulse bounds, register-block layout and watchdog minimum.
Firmware implementation, HSI accuracy and TIM1/TIM2 synchronization remain
validation items.

## ESP32-S3 GPIO allocation

Authoritative input: [PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md), which normatively incorporates the unchanged v0.2/v0.1 requirements; the existing calculations primarily trace to incorporated v0.1 §5.1. Calculations affected by the newly accepted v0.3 envelopes have not yet been rerun.

Source table: [esp32_gpio_assignment.csv](esp32_gpio_assignment.csv).

Run from the repository root:

    python3 hardware/electronics/calculations/verify_esp32_gpio_assignment.py

The check proves table completeness and uniqueness, the exact 15-pin expansion set, the three HALL_SHARED pins, the N16R8 Octal-PSRAM reservation, recovery pins, and the fixed onboard functions. It does not prove GPIO-matrix firmware configuration, signal integrity, analog accuracy, or assembly correctness.

Technical basis, accessed 2026-08-30:

- Espressif Systems, *ESP32-S3-WROOM-1 & ESP32-S3-WROOM-1U Datasheet*, v1.8, 2026-03-02: [ESP32-S3-WROOM-1-N16R8 module data](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.html).
- Espressif Systems, *ESP32-S3 Series Datasheet*, v2.2, 2026: [chip GPIO matrix, GPIO restrictions, and strap timing](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf). GPIO0/3/45/46 are strapping pins; the strap value is held for at least 3 ms after CHIP_PU rises, then the pins are released for normal GPIO use. GPIO46 is input/output capable, has a weak internal pull-down, and must be low or floating for joint download boot when GPIO0 is low.

Design interpretation:

- GPIO46 is assigned to PDM_CLK. The microphone clock pin is an input, an external 10 kΩ pull-down preserves the download-boot default, and firmware must not start the PDM clock until after reset strap sampling.
- GPIO3 is assigned to TCA_RESET_N with a 10 kΩ pull-down. With the default (unburned) JTAG-related eFuses, Espressif's table ignores GPIO3 and selects USB Serial/JTAG. Production programming must not change the JTAG strap eFuses without a new review.
- GPIO45 is exported as FREE_STRAP with a 10 kΩ pull-down. Any attached peripheral must remain high impedance or low during reset as required by the frozen interface.

## Low-voltage digital and I2C margins

Run from the repository root:

    python3 hardware/electronics/calculations/verify_low_voltage_digital_margins.py

The script explicitly evaluates these equations:

- Same-rail high-level margin: V_OH(min) − V_IH(min).
- Same-rail low-level margin: V_IL(max) − V_OL(max).
- I2C pull-up maximum: R_p(max) = t_r(max) / (0.8473 × C_b).
- I2C pull-up minimum: R_p(min) = (V_DD(max) − V_OL(max)) / I_OL.
- I2C rise time for the selected resistor: t_r = 0.8473 × R_p(max tolerance) × C_b.

Inputs and sources, accessed 2026-08-30:

- Espressif *ESP32-S3 Series Datasheet* v2.2: VDD 3.0–3.6 V, V_OH(min) = 0.8 × VDD and V_OL(max) = 0.1 × VDD under the stated high-impedance measurement condition; ESP input thresholds are 0.75/0.25 × VDD.
- Worldsemi *WS2812C-2020-V6*, v1.0, 2026-05-21: VDD 3.3–5.3 V, V_IH(min) = 0.55 × VDD, V_IL(max) = 0.7 V, 5 mA per color channel, four-pin order DO/GND/DI/VDD, and 100 nF local bypass recommendation. [Authorized-distributor-hosted manufacturer PDF](https://datasheet.lcsc.com/datasheet/pdf/8c7f104c99e52e4adb687ae84e594e95.pdf?productCode=C55109522).
- LinkMems *LMD3526B261-OFA03 Specification of MEMS Microphone*, M196 v1.0, released 2023-11-17: VDD 1.62–3.6 V; clock V_IH/V_IL = 0.65/0.35 × VDD; data V_OH(min) = VDD − 0.45 V and V_OL(max) = 0.45 V; standard-mode clock 1.0–4.8 MHz with 45–55% duty. [Authorized-distributor-hosted manufacturer PDF](https://atta.szlcsc.com/upload/public/pdf/source/20240812/81C7E69BC215A76B37A1D0E560FFDF0D.pdf).
- NXP *I2C-bus specification and user manual*, UM10204 Rev. 7.0, 2021-10-01: pull-up equations and Fast-mode 300 ns rise-time limit. [Manufacturer PDF](https://www.nxp.com/docs/en/user-guide/UM10204.pdf).

Design bounds:

- I2C is limited to 400 kHz Fast-mode and a total board-plus-short-expansion capacitance of 100 pF. The selected shared pull-ups are 2.20 kΩ ±1% to the local 3V3 rail. The capacitance assumption must be checked on the assembled prototype.
- PDM standard mode is fixed at 2.4 MHz. Clock and data each use a populated 33 Ω source-series resistor.
- Four LEDs are electrically budgeted at their full 60 mA channel sum even though firmware will limit brightness.
- The WS2812C and ESP32 guaranteed supply ranges overlap only from 3.3 V to 3.6 V. Therefore DIB-004 cannot be closed with a steady-state 3V3 lower limit below 3.3 V unless the LED supply architecture or frozen LED requirement changes.

## HAL49ESO ADC front end

Run from the repository root:

    python3 hardware/electronics/calculations/verify_hall_adc_frontend.py

Selected circuit for each populated Hall sensor:

    HAL49ESO OUT -- 27.4 kΩ ±1% -- ADC_NODE
                                      |
                                      +-- 100 kΩ ±1% -- GND
                                      |
                                      +-- 100 nF ±10% -- GND

For Hall 2–4, ADC_NODE reaches GPIO7/8/9 only through the independently frozen default-DNP 0 Ω option. Hall 1 reaches GPIO6 directly. Each Hall footprint has a local 100 nF VDD bypass; a shared 1 µF bulk capacitor is added near the Hall group.

Equations:

- Divider ratio k = R_bottom / (R_top + R_bottom).
- Worst high ratio uses R_bottom(max) and R_top(min).
- ADC upper-bound input = V_HALL(max bound) × k(max).
- Thevenin source resistance R_TH = R_top || R_bottom.
- RC cutoff f_c = 1 / (2π × R_TH × C).

Inputs and technical basis, accessed 2026-08-30:

- Hallwee Electronics, exact order suffix HAL49E SO (HAL49ESO), *HAL49E linear Hall sensor*, v3.2, publication date not stated. The SOT-23 SO package-specific note defines pin 1 power, pin 2 output, pin 3 ground. The stated supply range includes 3.3 V and the output test requires a load above 10 kΩ. [Authorized-distributor-hosted manufacturer PDF](https://datasheet.lcsc.com/datasheet/pdf/4c30f55e3c49f4ca4a72e58391bfc479df.pdf?productCode=C7420984).
- Espressif *ESP32-S3 Series Datasheet* v2.2: ADC1 is on GPIO1–10; ATTEN3 calibrated effective range extends to 2.9 V and its stated total error at that range is ±50 mV.

Worst-case electrical bounding uses 3.6 V as the maximum Hall output because the Hall is powered from the same rail. The manufacturer specifies sensitivity and output swing at 5 V rather than a complete 3.3 V transfer curve, so zero-field voltage, sensitivity, usable magnetic range, polarity, and calibration at the actual rail remain PVR-008 rather than being invented here.

## 3S battery UV/OV window

Run from the repository root:

    python3 hardware/electronics/calculations/verify_battery_uvov_window.py

Selected controller topology: Analog Devices LTC4368IMS-2#PBF, industrial-temperature MSOP-10, controlling external back-to-back N-channel MOSFETs. The -2 variant's reverse circuit-breaker threshold is −3 mV typical and −1 to −5 mV guaranteed, materially reducing battery-port backfeed compared with the -1 variant's −50 mV nominal threshold. RETRY is tied low so a forward overcurrent latches off; cycling MASTER_ENABLE through SHDN clears the latch.

The divider follows LTC4368 Rev. C Figure 4:

    VIN -- R3 1.66 MΩ -- UV -- R2 32.4 kΩ -- OV -- R1 67.8 kΩ -- GND

All three resistors are ±0.1%. The script solves KCL at UV and OV over every corner of resistor tolerance, the guaranteed 492.5–507.5 mV comparator threshold, 20–32 mV hysteresis, and independent ±10 nA UV/OV pin leakage. It checks that the maximum UV falling trip remains below the frozen 9.0 V input lower bound and the minimum OV rising trip remains above the frozen 12.6 V input upper bound. The deliberately wider UV hysteresis means a board that has shut down at low battery may require 8.958–9.523 V to re-enable; this prevents chatter and does not claim restart at exactly 9.0 V.

Source: Analog Devices, *LTC4368 100V UV/OV and Reverse Protection Controller with Bidirectional Circuit Breaker*, Rev. C, publication date not stated, accessed 2026-08-30. [Manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc4368.pdf).

This closes controller and voltage-window topology only. Design Input Freeze v0.3 has since closed DIB-001/002/005/006; physical fuse selection, exact current-sense resistance, MOSFET MPN/SOA, gate capacitor/inrush, total output capacitance, and branch protection remain pending engineering decisions and calculations. Those affected calculations have not yet been run.
