# Modular Carrier v0.2 module-photo evidence review

状态：**Reviewed input for schematic candidate; seller images are not as-built continuity evidence**

日期：2026-09-01

下表文件名是 Operator 提供的原始识别输入标识。按 core-only 仓库策略，原始商品图不纳入 Git；本文件保留已提炼的可用证据、限制和复核责任。

## Reviewed images

| File | Identification | Usable evidence | Remaining limitation |
| --- | --- | --- | --- |
| `5a0767a56881f1c3ee9dc71e0973d337.jpg` | dual-USB-C ESP32-S3-WROOM-1 development board, 2 × 22 headers; module marking/image indicates an N16R8-style variant | complete seller pin-label image; 2.54 mm pitch; row spacing shown as 25.40 mm and outside width 27.94 mm | not an actual-board front/back photograph; board length and onboard RGB GPIO remain unverified |
| `12f195efd106a28cea40d2603ed9ac45.jpg` | adjustable LM2596S module, board marking V151 | `IN+`, `IN-`, `OUT+`, `OUT-` visible | not the fixed-3.3 V variant image and no proof of 6 V-input regulation |
| `4dce493940d932f05efded9a44ab5590.jpg` | fixed 3.3 V LM2596S product listing, approximately 43 × 21 mm | image shows 3.3 V / 3 A and one visible 7–40 V statement; Operator subsequently confirmed the full product introduction specifies 3.2–40 V input and at least 1.5 V input/output headroom | exact purchased-module load regulation remains an acceptance-test item, but the stated range no longer blocks the 6 V architecture |
| `db583c7e122a320b9a6cc748b9987f7a.jpg` | common PCA9685 16-channel 12-bit PWM module | side header labels `GND/OE/SCL/SDA/VCC/V+`; screw-terminal V+; servo `PWM/V+/GND`; back legend states VCC/logic 3–5 V and V+ 6 V max; side control header is a right-angle header parallel to the module PCB | no manufacturer dimension drawing; image-scale estimate is approximately 68 × 28.5 mm with approximately 6.6 mm pin protrusion; actual purchased-board continuity/current behavior remains unverified |
| `ff82f9e05f1d14b4d515ac8064f3d065.jpg` | purple MAX98357A-compatible I²S amplifier with PH2.0 speaker output and 7-pin 2.54 mm header | seller wiring order: VCC, GND, pin 3 open, pin 4 open, DIN, BCLK, LRC; VCC accepts 3.3 V or 5 V; included speaker marked 4 Ω / 3 W; header is right-angle and parallel to the module PCB | exact amplifier IC marking and module current/thermal behavior are not readable; image-scale estimate is approximately 22 × 19 mm with approximately 6.1 mm pin protrusion |
| `f386f7d90316643a495b01393242f265.jpg` | round six-pad MS4030-class I²S MEMS microphone board | labels `SD/VDD/GND` and `L/R/WS/SCK` establish a digital I²S interface; images show the microphone package on the opposite face from the labelled pad view | actual microphone IC marking reads only approximately `NPC555`; supply range, board diameter, hole spacing and Z height are not printed, so electrical and mechanical fit both require actual-board verification |

Operator physical-orientation confirmation (2026-09-01): with the PCA9685 servo-output headers facing down and the green power terminal facing up, the terminal's left contact is `V+` and its right contact is `GND`. This orientation is accepted for the schematic candidate; the final footprint/silkscreen must preserve and visibly mark it.

## Mechanical evidence re-review — 2026-09-02

The PCA9685 and MAX98357A product images do not show straight board-to-board pins. Their supplied headers bend 90 degrees, with the pin axes parallel to the module PCBs. Inserting those pins into the current Carrier's vertical through-hole rows makes both module PCBs stand vertically.

Using the visible 2.54 mm header pitch only as an image scale gives the following non-manufacturer estimates:

| Module | Image-scale board estimate | Pin protrusion estimate | Current Carrier consequence |
| --- | --- | --- | --- |
| PCA9685 | approximately 68 × 28.5 mm | approximately 6.6 mm | direct-through-hole height approximately 70–75 mm depending insertion depth; socketed height approximately 78–81 mm pending exact socket |
| MAX98357A | approximately 22 × 19 mm | approximately 6.1 mm | direct-through-hole height approximately 24–28 mm depending insertion depth; socketed module approximately 32–35 mm before PH2.0 cable allowance |

These estimates identify a mechanical release risk but are not exact dimensions. The controlling review is [`PCB_MECHANICAL_FIT_REVIEW_2026-09-02.md`](PCB_MECHANICAL_FIT_REVIEW_2026-09-02.md); actual caliper measurements and vehicle clearances are required before manufacture.

## ESP32 restrictions applied to the schematic

The seller image exposes GPIO35–37, but the pictured/claimed N16R8 configuration uses octal PSRAM. Espressif documentation states that IO35–37 are not available for other use on ESP32-S3-WROOM-1 modules with octal PSRAM. The schematic labels these header pins `PSRAM_RESERVED`, not free GPIO.

The schematic also preserves these restrictions:

- GPIO0, GPIO3, GPIO45 and GPIO46: strapping pins; breakout only, no default external pull;
- GPIO19/GPIO20: native USB D-/D+; marked reserved;
- GPIO43/GPIO44: UART0/debug breakout;
- GPIO38/GPIO48: exposed as board-feature candidates, with onboard RGB assignment still requiring physical confirmation.

## Candidate I²S allocation

```text
GPIO39 = I2S_BCLK  -> MAX98357A BCLK + microphone SCK
GPIO40 = I2S_WS    -> MAX98357A LRC  + microphone WS
GPIO41 = I2S_AMP_DOUT -> MAX98357A DIN
GPIO47 = I2S_MIC_DIN  <- microphone SD
MIC L/R = 0 ohm to GND by default (left channel)
```

The two devices share clocks but use separate, oppositely directed data signals.

## LM2596 input-range resolution

The isolated fixed-3.3 V listing image appeared to state a 7–40 V input range. On 2026-09-01 the Operator supplied the fuller product-description limits:

```text
input voltage: 3.2–40 V
output voltage: 1.25–35 V
required headroom: VIN >= VOUT + 1.5 V
```

For this design:

```text
VIN nominal = 6.0 V
VOUT = 3.3 V
headroom = 6.0 V - 3.3 V = 2.7 V > 1.5 V
```

Thus nominal 6 V operation satisfies the Operator-confirmed product-description range and headroom, and the previous paper-design blocker is closed. The following remain ordinary module acceptance tests rather than schematic blockers:

1. startup from the minimum expected `EXT_6V_IN`;
2. regulated 3.3 V at representative logic load;
3. acceptable ripple and temperature;
4. behavior while servos disturb the 6 V rail.

The input rail should nevertheless remain above 4.8 V if a regulated 3.3 V output is expected under the stated 1.5 V headroom rule. Servo-rail disturbance testing therefore remains relevant even though nominal compatibility is established.
