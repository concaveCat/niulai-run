# EDD-023 test points and expansion protection v0.1

状态：**Connection set selected / exact protection MPNs pending**。测试点是测量节点，不是外部供电接口。

## 1. Mandatory power test points

| Net | Purpose |
| --- | --- |
| VBAT_CONN | battery connector positive before fuse |
| FUSE_OUT_RAW | after physical fuse / TVS node |
| VBAT_PROTECTED | after active battery protection |
| SERVO_BUCK_IN | SY8388 input |
| BUCK_6V | raw onboard buck output |
| SERVO_SOURCE | after 6 V/USB source isolation |
| SERVO_V+ | after common current shunt, at connector distribution entry |
| USB_PWR_VBUS_RAW | Type-C POWER VBUS before protection |
| USB5_PROTECTED | protected USB source |
| LOGIC_SOURCE | selected 5–12.6 V logic-buck input |
| CONTROL_AON_3V3 | always-on control domain |
| 3V3 | main logic rail |
| GND ×4 minimum | one by battery/power input, one by buck, one by USB, one by digital recovery area |

Power test points must be probe pads sized for oscilloscope/voltmeter use and positioned so a slipped probe cannot short adjacent high-current nodes. They are not connectors and must not be described as external power injection points.

## 2. Safety and power-status points

`MASTER_ON`, `ESTOP_SENSE`, `ESTOP_LOOP_OK`, `SERVO_FAULT_LATCH`, `SERVO_HW_ENABLE`, `BAT_FAULT_N`, `BUCK_EN`, `BUCK_PG`, `USB_PWR_FAULT_N`, `USB_CURRENT_MODE0/1`, `SERVO_CURRENT_ADC`, `VBAT_ADC`, and main `RESET_N` each receive an accessible signal test pad.

## 3. Programming and digital points

- ESP32: `EN`, `GPIO0/BOOT_N`, `UART0_TX`, `UART0_RX`, GND;
- CH32: `SWIO`, `NRST`, 3V3, GND;
- shared bus: `I2C_SCL`, `I2C_SDA`;
- PWM: `SERVO_PWM1` through `SERVO_PWM8` after 74HC244;
- IMU: `INT2` as already selected;
- audio: BCLK/LRCLK/DIN test pads only if routing access permits without stubs; otherwise series-resistor pads are measurement points.

## 4. Expansion GPIO protection

All fifteen frozen expansion GPIO remain connector-accessible. Each receives:

- a replaceable series element footprint near the ESP32, populated 100 Ω unless a documented interface requires 0/33 Ω;
- connector-side low-capacitance ESD protection to GND;
- no pull-up/down except the already frozen strap/default networks;
- silkscreen marking `3V3 ONLY / NOT 5V TOLERANT`;
- adjacent ground pins distributed across the connector rather than a single remote ground return.

GPIO45 remains externally constrained low/high-Z during reset; Hall-shared GPIO7/8/9 keep their independent default-DNP Hall links.

## 5. Expansion power

The external `3V3_PROTECTED` pin group is limited to the frozen 0.5 A total budget using a resettable protection device or current-limited load switch. It is default-off in USB 1.5 A minimum-logic mode and MASTER OFF. Exact protection MPN remains pending.

No raw battery, FUSE_OUT_RAW, SERVO_BUCK_IN, BUCK_6V or unprotected USB VBUS is exported on the expansion connector.

## 6. Temperature characterization

Rev A does not add a dedicated temperature-monitor IC. Provide one default-DNP 10 kΩ NTC footprint thermally close to the SY8388/inductor area with divider pads and an optional ADC link. Direct thermocouple attachment locations are identified in the later PCB review. This supports prototype thermal characterization without adding a required active component.
