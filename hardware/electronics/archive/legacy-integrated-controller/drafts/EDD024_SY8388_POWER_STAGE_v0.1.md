# EDD-024 SY8388ARHC 6 V power-stage design v0.1

状态：**Electrical values selected / exact passive MPNs pending**。本文件是 Circuit Design 计算草稿，不是 schematic 或 Circuit Design Freeze。

## 1. Fixed operating point

- controller: `SY8388ARHC / C5110279`, QFN2.5×2.5-16;
- input: `SERVO_BUCK_IN=9.0–12.6 V`;
- output: `BUCK_6V=6.0 V nominal`;
- load: 6 A continuous; 8 A for 1 s, event starts ≥10 s apart;
- switching frequency: datasheet 510–690 kHz, nominal 600 kHz;
- mode: default PFM (`MODE=GND`) to prevent light-load negative recirculation; provide a default-DNP 0 Ω option to AON/3V3 for FCCM prototype comparison;
- current-limit selection: `ILMT=GND` (low setting). The datasheet minimum valley-current threshold is 8 A; with the selected ripple, the expected output-current limit is above the required 8 A transient but remains a prototype corner to verify.

## 2. Feedback and local mandatory capacitors

| Pin/network | Selected circuit-design value |
| --- | --- |
| FB upper resistor | 100 kΩ ±0.1% from BUCK_6V to FB |
| FB lower resistor | 11.1 kΩ ±0.1% from FB to quiet GND |
| Output corner | 5.935–6.076 V using VREF and resistor tolerance |
| BS | 100 nF X7R between BS and LX |
| VCC | 2.2 µF X7R to quiet/power GND as manufacturer requires |
| BYP | tied to GND; external 3.3 V bypass is not used in Rev A |
| PG | 100 kΩ pull-up to main 3V3; routed to status input/test point; no safety function |
| EN | external 100 kΩ pull-down; hardware servo-enable chain may only pull it high through a bounded resistor, never directly to VIN |

FB divider and VCC/BYP capacitors must be referenced to the exposed-pad quiet ground area, away from LX/BS.

## 3. Inductor

Select `2.7 µH ±20%` with:

- saturation current ≥12 A at the relevant temperature and manufacturer-defined inductance-drop criterion;
- RMS/temperature-rise current ≥8.5 A at 40 °C ambient;
- DCR ≤8 mΩ at 25 °C, with hot DCR included in final loss calculation;
- shielded ferrite construction.

At 12.6 V, 510 kHz and Lmin=2.16 µH:

- ripple ≈2.85 A, 35.7% of the 8 A load;
- 8 A peak ≈9.43 A;
- 8 A RMS ≈8.04 A.

This brings the worst tolerance corner inside Silergy's 20–40% ripple design range; the older 2.2 µH screen reached about 43.8% and is no longer preferred.

## 4. Input capacitance requirement

The installed input network must provide, at 12.6 V DC bias and 40 °C:

- effective ceramic capacitance ≥40 µF at the SY8388 IN/GND pins;
- combined RMS ripple-current rating ≥4 A;
- voltage rating ≥25 V;
- at least one 100 nF and one 1 µF X7R directly adjacent to the IN pins;
- optional bulk polymer/electrolytic only after the battery-protection inrush calculation is closed.

At 8 A, 510 kHz and 40 µF effective, the first-order capacitive input ripple bound is about 98 mV. Exact capacitor MPNs must be checked using DC-bias curves, not nominal capacitance alone.

## 5. Output capacitance requirement

The installed output network must provide at 6.0 V bias and 40 °C:

- effective capacitance ≥330 µF after tolerance and DC-bias derating;
- parallel ESR ≤10 mΩ over the relevant switching/servo transient band;
- voltage rating ≥10 V;
- ripple-current and temperature ratings compatible with 6 A continuous/8 A transient.

The worst first-order 0→8 A, 0.20 V capacitive undershoot calculation with Lmax=3.24 µH requires approximately 269 µF effective. The selected ≥330 µF requirement provides about 23% capacitance margin. With 2.85 A ripple, 510 kHz, 330 µF and 10 mΩ total ESR, the switching-ripple screen is ≤30.6 mV.

An economical starting network is two 220 µF/10 V low-ESR polymer capacitors plus local ceramics, provided the exact MPNs still deliver ≥330 µF minimum and ≤10 mΩ combined ESR. Total effective capacitance near or above the manufacturer's 500 µF compensation boundary requires rechecking the recommended RFF/CFF network; it must not be silently omitted.

## 6. Pin-by-pin circuit-design table

| Pin | Name | Circuit-design connection |
| ---: | --- | --- |
| 1 | BS | 100 nF to LX |
| 2,3,4 | IN | SERVO_BUCK_IN; input capacitors immediately to GND |
| 5,15,16 | LX | common switch node to 2.7 µH inductor; smallest copper area consistent with current/thermal needs |
| 6,14,EP | GND | uninterrupted power ground and exposed-pad thermal-via field |
| 7 | PG | 100 kΩ to 3V3, status/test point |
| 8 | MODE | GND default; DNP 0 Ω option to 3V3 for FCCM test |
| 9 | EN | 100 kΩ to GND; hardware `SERVO_HW_ENABLE` drive only |
| 10 | ILMT | GND |
| 11 | FB | 100 kΩ/11.1 kΩ divider midpoint; Kelvin sense at output capacitor |
| 12 | BYP | GND |
| 13 | VCC | 2.2 µF X7R to GND |

## 7. Thermal and layout constraints

- four-layer PCB, outer-layer 2 oz copper as assumed by the current design input;
- continuous ground plane; do not split logic and power grounds;
- at least four low-inductance ground/thermal vias immediately in the exposed-pad field, with additional stitching as fabrication permits;
- CIN–IN–high-side switch–GND hot loop and LX–inductor–COUT loop kept compact;
- FB trace Kelvin-routed from COUT positive and shielded from LX/BS;
- no sensitive microphone/IMU/antenna routing beneath or adjacent to LX/inductor;
- provide `SERVO_BUCK_IN`, `BUCK_6V`, PG, EN and GND measurement points without creating an external injection connector.

Exact inductor/capacitor loss, total converter efficiency, junction temperature and EMI remain open until exact passive MPNs are provided. Prototype validation PVR-015 remains mandatory.

Reproduce the current screen with:

```sh
python3 hardware/electronics/calculations/verify_sy8388_buck.py
```
