# EDD-015 CH32V003 servo PWM engine protocol v0.1

状态：**Circuit/firmware interface selected / firmware validation pending**。本文定义电路设计必须支持的协议和默认状态，不是 firmware implementation 或 schematic。

## 1. Fixed electrical mapping

CH32V003F4P6 使用 3V3、内部 24 MHz HSI 经 ×2 形成 48 MHz timer clock。八路正常输出全部来自硬件 timer：

| Servo | CH32 output | 74HC244 input/output | Connector net |
| --- | --- | --- | --- |
| 1 | TIM1_CH1 / PD2 | pin 2 `1A1` → pin 18 `1Y1` | `SERVO_PWM1` |
| 2 | TIM1_CH2 / PA1 | pin 4 `1A2` → pin 16 `1Y2` | `SERVO_PWM2` |
| 3 | TIM1_CH3 / PC3 | pin 6 `1A3` → pin 14 `1Y3` | `SERVO_PWM3` |
| 4 | TIM1_CH4 / PC4 | pin 8 `1A4` → pin 12 `1Y4` | `SERVO_PWM4` |
| 5 | TIM2_CH1 / PC1 | pin 11 `2A1` → pin 9 `2Y1` | `SERVO_PWM5` |
| 6 | TIM2_CH2 / PC7 | pin 13 `2A2` → pin 7 `2Y2` | `SERVO_PWM6` |
| 7 | TIM2_CH3 / PD6 | pin 15 `2A3` → pin 5 `2Y3` | `SERVO_PWM7` |
| 8 | TIM2_CH4 / PD5 | pin 17 `2A4` → pin 3 `2Y4` | `SERVO_PWM8` |

74HC244 pin 20=`3V3`, pin 10=`GND`; pins 1 and 19 (`/1OE`, `/2OE`) are joined as `SERVO_PWM_OE_N`, pulled up by 10 kΩ to 3V3 and connected through 1 kΩ to TCA9539 P15. TCA P15 must be configured output-low only after safe PWM initialization and servo-arm conditions are satisfied.

Each 74HC244 A input and each connector-side Y output has an independent 100 kΩ pull-down. Every Y output has an accessible test point. No output is allowed to float into a valid servo pulse during reset, unprogrammed CH32, TCA reset or OE open-circuit.

TCA9539 allocation retained by this design:

| TCA pin | Function |
| --- | --- |
| P00–P03 | FOOT1_N–FOOT4_N |
| P04 | BAT_FAULT_N |
| P05 | BUCK_PG |
| P06 | USB_PWR_FAULT_N |
| P07 | ESTOP_STATUS_N |
| P10 | USB_DATA_VBUS_PRESENT_N |
| P11/P13 | USB_CURRENT_MODE[0:1] |
| P12 | SCREEN_BL_ENABLE |
| P14 | AMP_ENABLE |
| P15 | SERVO_PWM_OE_N control; output-low enables |
| P16 | IMU_INT1 |
| P17 | LOW_SPEED_GPIO |

## 2. I2C interface

- 7-bit address: `0x40`;
- bus modes: 100 kHz and 400 kHz;
- pins: PC5=SCL, PC6=SDA using remap; shared board pull-ups remain 2.20 kΩ ±1%;
- multi-byte values: little-endian;
- malformed/partial writes never change active PWM compare registers;
- no clock stretching longer than 1 ms; a bus timeout resets only the I2C peripheral and does not enable outputs.

## 3. Minimal register protocol

| Address | Name | Access | Meaning |
| ---: | --- | --- | --- |
| 0x00 | DEVICE_ID | R | fixed `0x51` |
| 0x01 | PROTOCOL_VERSION | R | `0x01` |
| 0x02 | STATUS | R | bit0 READY; bit1 PWM_RUNNING; bit2 WATCHDOG_FAULT; bit3 RANGE_ERROR; bit4 I2C_ERROR |
| 0x03 | CONTROL | R/W | bit0 RUN_REQUEST; writing 0 stops timers and drives all PWM pins low |
| 0x04 | WATCHDOG_10MS | R/W | default 10 = 100 ms; legal 2..100; 0 is not allowed |
| 0x05 | ACTIVE_SEQUENCE | R | increments after a successful commit |
| 0x06–0x07 | RESERVED | — | read zero; writes ignored |
| 0x08–0x17 | PWM1..8_SHADOW | R/W | eight uint16 pulse widths in microseconds |
| 0x18 | COMMIT | W | write `0xA5` to validate/clamp and transfer all eight shadow values atomically at the next frame boundary |
| 0x19 | COMMAND | W | write `0xDE` for immediate DISARM: timers off, pins low, watchdog fault retained |

Pulse values outside 500–2500 µs cause `RANGE_ERROR`; the commit is rejected rather than silently wrapping. On reset, shadow values may contain 1500 µs neutral, but active PWM is disabled and physical outputs remain low.

## 4. Timer contract

At a 48 MHz timer clock:

```text
PSC = 47       → timer tick = 1 MHz = 1 µs
ARR = 19999    → frame = 20.000 ms = 50 Hz
CCR1..4        → pulse width directly in microseconds
```

TIM1 and TIM2 must start on a common hardware/software synchronization event with frame-phase skew ≤2 µs. Preload registers must be enabled, and a valid COMMIT transfers both timers at the same 20 ms frame boundary. Normal operation may not bit-bang any PWM output.

The internal HSI accuracy directly becomes pulse-width/frame-frequency accuracy and must be checked in firmware/prototype validation. An external crystal is not added unless that test fails.

## 5. Watchdog and safety behavior

- Default watchdog: 100 ms, refreshed only by a valid complete I2C transaction from ESP32;
- timeout: clear RUN_REQUEST, disable both timers, reconfigure all eight PWM pins low, set WATCHDOG_FAULT;
- recovery: valid shadow write + COMMIT + explicit RUN_REQUEST; watchdog recovery alone does not pull `SERVO_PWM_OE_N` low;
- TCA P15 remains the independent board-level output gate;
- external ESTOP and servo-power shutdown never depend on CH32 firmware.

## 6. Programming and test access

Provide accessible test points for `CH32_SWIO`, `CH32_NRST`, `3V3`, `GND`, `I2C_SCL`, `I2C_SDA`, and all eight buffered PWM outputs. `NRST` has a passive pull-up and local capacitor only if allowed by WCH programming timing; exact values remain part of the pin-by-pin connection table.

Run the static contract check with:

```sh
python3 hardware/electronics/calculations/verify_ch32_pwm_protocol.py
```
