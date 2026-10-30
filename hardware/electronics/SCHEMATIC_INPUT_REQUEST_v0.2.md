# Modular Carrier v0.2 schematic input request

状态：**Operator evidence checklist before schematic capture**

日期：2026-09-01

权威架构：[`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md)

## A. Required before an electrically complete schematic

For every purchased module, provide a purchase link or exact model, clear front/back photographs with all pin labels visible, and a pin table. A ruler/caliper photograph is required only for modules that receive a dedicated Carrier socket.

| Item | Required evidence | Why it blocks |
| --- | --- | --- |
| ESP32 development board | exact model/revision, official pinout, front/back photos, header pitch/spacing, all exposed GPIOs, intended USB use | fixes power pins, boot/strap restrictions, GPIO allocation and the complete unused-GPIO breakout map |
| ESP32 power policy | confirm whether Carrier drives the board's `3V3` pin directly; state whether USB may be connected while external 3V3 is on | prevents regulator/USB back-power and determines any jumper/isolation requirement |
| PCA9685 module | exact board/version, front/back photos, VCC/V+/GND/SDA/SCL/OE pin order, address-jumper state, servo header orientation | fixes safe-default control and eight-servo wiring |
| LM2596S module | front/back photos, IN+/IN-/OUT+/OUT- order, dimensions/header locations, confirmation it can be adjusted to 3.3 V from 6 V input | fixes logic-power connector and mounting |
| MAX98357A module | exact board, pinout, accepted VIN, SD/EN and GAIN wiring, speaker connector, intended speaker impedance/power | fixes I²S and audio-power wiring |
| MS4030 module | exact board and pin labels; state whether output is analog, amplified analog or digital | interface cannot be assigned safely from the name alone |
| Servo distribution | confirm `Option A first build + Option B fallback footprints`, or choose only one; provide PCA V+ inspection if using Option A | determines whether Carrier has eight complete servo power connectors or only fallback pads/headers |
| Foot interfaces | confirm switch is on Carrier or remote by wire; desired Carrier connector style/pin count | fixes connector symbols and pull-up/filter placement |

## B. Required for wiring documentation, but not Carrier footprints

MPU6500 and ST7789 use Dupont flying wires through `J_GPIO_BREAKOUT`, so their dimensions and mounting holes do not block Carrier design. Still provide:

- clear pin-label photographs or pinout links;
- confirmed supply voltage accepted at the module header;
- MPU6500 choice of SPI or I²C;
- ST7789 backlight pin voltage/polarity and whether the board includes a resistor/transistor;
- desired cable connector gender and approximate wire length.

These facts populate the external wiring table and prevent incorrect flying-wire connections.

## C. May remain generic during schematic capture

The following do not block the first electrical schematic and may receive generic symbols plus `FOOTPRINT_TBD`:

- exact board-mount form/MPN of the frozen XT30 male `J_PWR`;
- exact ATO/ATC holder footprint;
- C_BULK1/C_BULK2 manufacturer and body height;
- final board outline, mounting holes and module coordinates;
- exact test-point footprint.

They must be frozen before PCB footprint placement and manufacturing output.

## D. Preferred evidence format

Create one directory per module:

```text
evidence/modules/ESP32/
evidence/modules/PCA9685/
evidence/modules/LM2596S/
evidence/modules/MAX98357A/
evidence/modules/MS4030/
evidence/modules/MPU6500/
evidence/modules/ST7789/
```

Each directory should contain, where available:

```text
front.jpg
back.jpg
dimensions.jpg
datasheet.pdf or purchase-link.txt
pinout.md
```

Do not include account information, order numbers, addresses or other personal data in screenshots.

## E. Planned schematic deliverables

After Section A is complete, the circuit-design deliverable can include:

- multi-sheet schematic: power/protection, ESP32/GPIO breakout, PCA/servo, foot sensors, audio/microphone and module connectors;
- importable KiCad project/archive with project-local symbols;
- PDF/SVG schematic preview;
- machine-checkable net/interface table and BOM draft;
- unresolved-footprint report and ERC/electrical review report;
- EasyEDA import/review checklist.

Import success does not prove electrical correctness. The imported project must be checked against the interface table, especially symbol pin numbers, power symbols, no-connect flags, custom symbols and footprint binding.
