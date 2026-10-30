#!/usr/bin/env python3
"""Generate the Niulai Carrier v0.2 KiCad schematic and review artifacts.

The generated schematic intentionally uses embedded project-local symbols so the
archive does not depend on a workstation's symbol-library table.  Electrical
connectivity is also emitted as CSV and independently checked by the companion
verification script.
"""

from __future__ import annotations

import csv
import json
import uuid
import zipfile
from dataclasses import dataclass
from pathlib import Path


OUT = Path(__file__).resolve().parent
PROJECT = "niulai_carrier_v0.2"
ROOT_UUID = uuid.uuid5(uuid.NAMESPACE_URL, "Niulai/Carrier/v0.2/root")


@dataclass(frozen=True)
class Component:
    ref: str
    value: str
    pins: tuple[tuple[str, str | None], ...]
    x: float
    y: float
    footprint: str = "FOOTPRINT_TBD"
    description: str = ""
    dnp: bool = False
    in_bom: bool = True
    left_count: int = 0


def esc(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def sid(*parts: str) -> str:
    return str(uuid.uuid5(ROOT_UUID, "/".join(parts)))


def esp_headers() -> tuple[tuple[str, str], tuple[str, str]]:
    left = (
        ("3V3_A", "ESP_3V3"),
        ("3V3_B", "ESP_3V3"),
        ("RST NC", None),
        ("GPIO4", "PCA_OE"),
        ("GPIO5", "FOOT_FL"),
        ("GPIO6", "FOOT_FR"),
        ("GPIO7", "FOOT_RL"),
        ("GPIO15", "FOOT_RR"),
        ("GPIO16", "DISPLAY_RESET"),
        ("GPIO17", "DISPLAY_BL"),
        ("GPIO18", "IMU_CS"),
        ("GPIO8", "I2C_SDA"),
        ("GPIO3_STRAP", "GPIO3_STRAP_AUX"),
        ("GPIO46_STRAP", "GPIO46_STRAP_AUX"),
        ("GPIO9", "I2C_SCL"),
        ("GPIO10", "DISPLAY_CS"),
        ("GPIO11", "SPI_MOSI"),
        ("GPIO12", "SPI_SCLK"),
        ("GPIO13", "SPI_MISO"),
        ("GPIO14", "DISPLAY_DC"),
        ("5V USB_ONLY NC", None),
        ("GND", "GND"),
    )
    right = (
        ("GND_A", "GND"),
        ("GPIO43_TX", "GPIO43_UART_TX_AUX"),
        ("GPIO44_RX", "GPIO44_UART_RX_AUX"),
        ("GPIO1", "GPIO1_AUX"),
        ("GPIO2", "GPIO2_AUX"),
        ("GPIO42", "GPIO42_AUX"),
        ("GPIO41", "I2S_AMP_DOUT"),
        ("GPIO40", "I2S_WS"),
        ("GPIO39", "I2S_BCLK"),
        ("GPIO38", "GPIO38_BOARD_AUX"),
        ("GPIO37 PSRAM_RESERVED NC", None),
        ("GPIO36 PSRAM_RESERVED NC", None),
        ("GPIO35 PSRAM_RESERVED NC", None),
        ("GPIO0_BOOT", "GPIO0_BOOT_AUX"),
        ("GPIO45_STRAP", "GPIO45_STRAP_AUX"),
        ("GPIO48", "GPIO48_BOARD_AUX"),
        ("GPIO47", "I2S_MIC_DIN"),
        ("GPIO21", "IMU_INT"),
        ("GPIO20 USB_D+ RESERVED NC", None),
        ("GPIO19 USB_D- RESERVED NC", None),
        ("GND_B", "GND"),
        ("GND_C", "GND"),
    )
    return left, right


def physical_pins(prefix: str, pins: tuple[tuple[str, str], ...]) -> tuple[tuple[str, str], ...]:
    return tuple((f"{prefix}{index:02d} {name}", net) for index, (name, net) in enumerate(pins, start=1))


def components() -> list[Component]:
    esp_left, esp_right = esp_headers()
    esp_module = physical_pins("L", esp_left) + physical_pins("R", esp_right)
    gpio_breakout = (
        ("01 3V3", "3V3"), ("02 GND", "GND"),
        ("03 GPIO16 DISPLAY_RESET", "DISPLAY_RESET"), ("04 GPIO17 DISPLAY_BL", "DISPLAY_BL"),
        ("05 GPIO18 IMU_CS", "IMU_CS"), ("06 GPIO10 DISPLAY_CS", "DISPLAY_CS"),
        ("07 GPIO11 SPI_MOSI", "SPI_MOSI"), ("08 GPIO12 SPI_SCLK", "SPI_SCLK"),
        ("09 GPIO13 SPI_MISO", "SPI_MISO"), ("10 GPIO14 DISPLAY_DC", "DISPLAY_DC"),
        ("11 GPIO21 IMU_INT", "IMU_INT"), ("12 GPIO1 AUX", "GPIO1_AUX"),
        ("13 GPIO2 AUX", "GPIO2_AUX"), ("14 GPIO42 AUX", "GPIO42_AUX"),
        ("15 GPIO43 UART_TX", "GPIO43_UART_TX_AUX"), ("16 GPIO44 UART_RX", "GPIO44_UART_RX_AUX"),
        ("17 GPIO38 BOARD_AUX", "GPIO38_BOARD_AUX"), ("18 GPIO48 BOARD_AUX", "GPIO48_BOARD_AUX"),
        ("19 GPIO0 BOOT_STRAP", "GPIO0_BOOT_AUX"), ("20 GPIO3 STRAP", "GPIO3_STRAP_AUX"),
        ("21 GPIO45 STRAP", "GPIO45_STRAP_AUX"), ("22 GPIO46 STRAP", "GPIO46_STRAP_AUX"),
    )
    pca_control = (
        ("H01 GND", "GND"), ("H02 OE", "PCA_OE"), ("H03 SCL", "I2C_SCL"),
        ("H04 SDA", "I2C_SDA"), ("H05 VCC", "3V3"), ("H06 V+", "SERVO_6V"),
    )
    pca_outputs = (("PWR+ V+ VERIFY", "SERVO_6V"), ("PWR- GND VERIFY", "GND")) + tuple(
        pin
        for channel in range(8)
        for pin in (
            (f"CH{channel}-S PWM", f"SERVO_PWM{channel + 1}"),
            (f"CH{channel}-+ V+", "SERVO_6V"),
            (f"CH{channel}-- GND", "GND"),
        )
    )
    items = [
        Component("J1", "XT30_MALE_BOARD_INPUT", (("+6V", "INPUT_6V_UNFUSED"), ("GND", "GND")), 28, 35,
                  "FOOTPRINT_TBD_XT30_MALE", "Board side male; energized source cables female"),
        Component("F1", "ATO_ATC_10A_32V_MIN", (("IN", "INPUT_6V_UNFUSED"), ("OUT", "EXT_6V_IN")), 72, 35,
                  "FOOTPRINT_TBD_ATO_ATC_HOLDER_15A", "Not a precision current limiter"),
        Component("NT1", "PCB_COPPER_SERVO_BRANCH_10A", (("MAIN", "EXT_6V_IN"), ("SERVO", "SERVO_6V")), 116, 35,
                  "FOOTPRINT_TBD_10A_NET_TIE", "Logical domain boundary; continuous wide copper", in_bom=False),
        Component("U5", "LM2596S_MODULE_V151", (("IN+", "EXT_6V_IN"), ("IN-", "GND"), ("OUT+", "3V3"), ("OUT-", "GND")), 162, 35,
                  "FOOTPRINT_TBD_LM2596S_43x21", "Photo-visible order; VIN 3.2-40V and VIN >= VOUT+1.5V", left_count=2),
        Component("C1", "1000-2200uF_16V_105C_OPTIONAL", (("+", "SERVO_6V"), ("-", "GND")), 215, 35,
                  "Capacitor_THT:C_Radial_D12.5mm_H20.0mm_P5.00mm", "C_BULK1", dnp=True),
        Component("C2", "1000-2200uF_16V_105C_OPTIONAL", (("+", "SERVO_6V"), ("-", "GND")), 258, 35,
                  "Capacitor_THT:C_Radial_D12.5mm_H20.0mm_P5.00mm", "C_BULK2", dnp=True),
        Component("C3", "10uF_16V_X7R", (("1", "3V3"), ("2", "GND")), 300, 35,
                  "Capacitor_SMD:C_1206_3216Metric", "Carrier local rail decoupling"),
        Component("C4", "100nF_16V_X7R", (("1", "3V3"), ("2", "GND")), 340, 35,
                  "Capacitor_SMD:C_0805_2012Metric", "Carrier local high-frequency decoupling"),
        Component("JP1", "ESP_EXTERNAL_3V3_LINK_REMOVE_FOR_USB_ONLY_TEST", (("SYS", "3V3"), ("ESP", "ESP_3V3")), 28, 85,
                  "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical", "Avoid unreviewed USB/external-rail paralleling"),
        Component("U1", "ESP32_S3_N16R8_DEVBOARD_2x22", esp_module, 100, 130,
                  "FOOTPRINT_TBD_ESP32_S3_2x22_SOCKET", "One complete module; L/R numbering follows seller image antenna-to-USB", left_count=22),
        Component("J3", "GPIO_DUPONT_BREAKOUT", gpio_breakout, 190, 130,
                  "FOOTPRINT_TBD_GPIO_HEADER", "MPU6500/ST7789 plus all final unused GPIO; exact header form TBD"),
        Component("U2", "PCA9685_16CH_MODULE_OPTION_A", pca_control + pca_outputs, 300, 105,
                  "FOOTPRINT_TBD_PCA9685_MODULE", "Green terminal: outputs down/terminal up => left V+, right GND", left_count=6),
        Component("R1", "10k_1%_OE_SAFE_DEFAULT", (("1", "3V3"), ("2", "PCA_OE")), 420, 90,
                  "Resistor_SMD:R_0805_2012Metric", "OE high disables PWM during reset"),
        Component("R2", "4.7k_I2C_PULLUP_DNP", (("1", "3V3"), ("2", "I2C_SDA")), 462, 82,
                  "Resistor_SMD:R_0805_2012Metric", "Populate only if module pull-up audit requires", dnp=True),
        Component("R3", "4.7k_I2C_PULLUP_DNP", (("1", "3V3"), ("2", "I2C_SCL")), 462, 100,
                  "Resistor_SMD:R_0805_2012Metric", "Populate only if module pull-up audit requires", dnp=True),
        Component("U3", "MAX98357A_PURPLE_MODULE", (("1 VCC", "3V3"), ("2 GND", "GND"), ("3 NC", None), ("4 NC", None), ("5 DIN", "I2S_AMP_DOUT"), ("6 BCLK", "I2S_BCLK"), ("7 LRC", "I2S_WS"), ("SPK+", "SPK_PLUS"), ("SPK-", "SPK_MINUS")), 285, 205,
                  "FOOTPRINT_TBD_MAX98357A_MODULE", "Complete 7-pin plus PH2.0 speaker module", left_count=7),
        Component("U4", "MS4030_I2S_MIC_MODULE", (("T1 SD", "I2S_MIC_DIN"), ("T2 VDD", "3V3"), ("T3 GND", "GND"), ("B1 L/R", "MIC_LR"), ("B2 WS", "I2S_WS"), ("B3 SCK", "I2S_BCLK")), 350, 205,
                  "FOOTPRINT_TBD_MS4030_FLYWIRE", "Photo pad order top SD/VDD/GND; bottom L-R/WS/SCK"),
        Component("R4", "0R_MIC_LEFT_CHANNEL", (("1", "MIC_LR"), ("2", "GND")), 430, 145,
                  "Resistor_SMD:R_0805_2012Metric", "Move strap to 3V3 only for right-channel configuration"),
        Component("SP1", "EXTERNAL_SPEAKER_4R_3W", (("SPK+", "SPK_PLUS"), ("SPK-", "SPK_MINUS")), 340, 235,
                  "", "Off-board speaker via module PH2.0; neither lead is GND", in_bom=False, left_count=2),
    ]

    for channel in range(8):
        items.append(Component(
            f"M{channel + 1}", f"EXTERNAL_SERVO_{channel + 1}",
            (("S PWM", f"SERVO_PWM{channel + 1}"), ("+ 6V", "SERVO_6V"), ("- GND", "GND")),
            380, 55 + channel * 12.7, "", "Off-board servo plugs into PCA9685 channel header",
            in_bom=False, left_count=3,
        ))

    foot_names = ("FOOT_FL", "FOOT_FR", "FOOT_RL", "FOOT_RR")
    for index, net in enumerate(foot_names, start=1):
        x = 45 + (index - 1) * 62
        items.extend([
            Component(f"J{9 + index}", f"{net}_REMOTE_SWITCH", (("SIGNAL", net), ("GND", "GND")), x, 225,
                      "FOOTPRINT_TBD_2PIN", "Remote switch closes signal to GND"),
            Component(f"R{4 + index}", "10k_1%", (("1", "3V3"), ("2", net)), x, 245,
                      "Resistor_SMD:R_0805_2012Metric", "Active-low pull-up"),
            Component(f"C{4 + index}", "100nF_FILTER_DNP", (("1", net), ("2", "GND")), x, 265,
                      "Capacitor_SMD:C_0805_2012Metric", "Optional hardware debounce", dnp=True),
        ])

    test_nets = ("GND", "EXT_6V_IN", "SERVO_6V", "3V3", "I2C_SDA", "I2C_SCL", "PCA_OE")
    for index, net in enumerate(test_nets, start=1):
        items.append(Component(f"TP{index}", f"TEST_{net}", (("1", net),), 60 + (index - 1) * 62, 300,
                               "TestPoint:TestPoint_Plated_Hole_D2.0mm", "Accessible after module installation"))
    return items


def prop(name: str, value: str, x: float, y: float, hidden: bool = False) -> str:
    hide = "\n\t\t\t\t(hide yes)" if hidden else ""
    return f'''\t\t(property "{esc(name)}" "{esc(value)}"
\t\t\t(at {x:.3f} {y:.3f} 0)
\t\t\t(effects
\t\t\t\t(font (size 1.27 1.27)){hide}
\t\t\t)
\t\t)'''


def pin_number(pin_name: str, index: int) -> str:
    token = pin_name.split()[0]
    return token if token[0].isalpha() else str(index)


def side_position(comp: Component, index: int) -> tuple[str, int, int]:
    if comp.left_count and index <= comp.left_count:
        return "left", index, comp.left_count
    return "right", index - comp.left_count, len(comp.pins) - comp.left_count


def lib_symbol(comp: Component) -> str:
    base = f"{comp.ref}_{comp.value}"
    name = f"Niulai:{base}"
    half_height = max(5.08, (max(comp.left_count, len(comp.pins) - comp.left_count) - 1) * 1.27 + 2.54)
    pin_chunks = []
    for index, (pin_name, _net) in enumerate(comp.pins, start=1):
        side, side_index, side_count = side_position(comp, index)
        y = ((side_count + 1) / 2 - side_index) * 2.54
        x = -15.24 if side == "left" else 15.24
        angle = 0 if side == "left" else 180
        pin_chunks.append(f'''\t\t\t\t(pin passive line
\t\t\t\t\t(at {x:.3f} {y:.3f} {angle})
\t\t\t\t\t(length 5.08)
\t\t\t\t\t(name "{esc(pin_name)}" (effects (font (size 1.0 1.0))))
\t\t\t\t\t(number "{esc(pin_number(pin_name, index))}" (effects (font (size 1.0 1.0))))
\t\t\t\t)''')
    return f'''\t\t(symbol "{esc(name)}"
\t\t\t(pin_names (offset 1.27))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom {'yes' if comp.in_bom else 'no'})
\t\t\t(on_board yes)
{prop('Reference', comp.ref[0] if not comp.ref.startswith('TP') else 'TP', 0, -half_height - 3.81)}
{prop('Value', comp.value, 0, half_height + 3.81)}
{prop('Footprint', comp.footprint, 0, 0, True)}
{prop('Datasheet', '', 0, 0, True)}
{prop('Description', comp.description, 0, 0, True)}
\t\t\t(symbol "{esc(base)}_0_1"
\t\t\t\t(rectangle
\t\t\t\t\t(start -10.0 {-half_height:.3f})
\t\t\t\t\t(end 10.0 {half_height:.3f})
\t\t\t\t\t(stroke (width 0.254) (type default))
\t\t\t\t\t(fill (type background))
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "{esc(base)}_1_1"
{chr(10).join(pin_chunks)}
\t\t\t)
\t\t)'''


def label(net: str, x: float, y: float, key: str, angle: int = 0) -> str:
    justify = "left bottom" if angle == 0 else "right bottom"
    return f'''\t(label "{esc(net)}"
\t\t(at {x:.3f} {y:.3f} {angle})
\t\t(effects (font (size 1.0 1.0)) (justify {justify}))
\t\t(uuid "{sid('label', key)}")
\t)'''


def wire(x1: float, y1: float, x2: float, y2: float, key: str) -> str:
    return f'''\t(wire
\t\t(pts
\t\t\t(xy {x1:.3f} {y1:.3f})
\t\t\t(xy {x2:.3f} {y2:.3f})
\t\t)
\t\t(stroke (width 0) (type default))
\t\t(uuid "{sid('wire', key)}")
\t)'''


def no_connect(x: float, y: float, key: str) -> str:
    return f'''\t(no_connect
\t\t(at {x:.3f} {y:.3f})
\t\t(uuid "{sid('nc', key)}")
\t)'''


def stub_point(comp: Component, wanted_pin: str) -> tuple[float, float]:
    sx = round(comp.x / 1.27) * 1.27
    sy = round(comp.y / 1.27) * 1.27
    for index, (pin_name, net) in enumerate(comp.pins, start=1):
        if pin_number(pin_name, index) != wanted_pin:
            continue
        assert net is not None
        side, side_index, side_count = side_position(comp, index)
        y = sy - ((side_count + 1) / 2 - side_index) * 2.54
        x = sx - 22.86 if side == "left" else sx + 22.86
        return x, y
    raise KeyError((comp.ref, wanted_pin))


def route_via_y(a: tuple[float, float], b: tuple[float, float], lane_y: float, key: str) -> list[str]:
    return [
        wire(a[0], a[1], a[0], lane_y, f"{key}/a"),
        wire(a[0], lane_y, b[0], lane_y, f"{key}/lane"),
        wire(b[0], lane_y, b[0], b[1], f"{key}/b"),
    ]


def route_via_x(a: tuple[float, float], b: tuple[float, float], lane_x: float, key: str) -> list[str]:
    return [
        wire(a[0], a[1], lane_x, a[1], f"{key}/a"),
        wire(lane_x, a[1], lane_x, b[1], f"{key}/lane"),
        wire(lane_x, b[1], b[0], b[1], f"{key}/b"),
    ]


def route_outer(a: tuple[float, float], b: tuple[float, float], escape_a: float, escape_b: float, lane_y: float, key: str) -> list[str]:
    return [
        wire(a[0], a[1], escape_a, a[1], f"{key}/a-escape"),
        wire(escape_a, a[1], escape_a, lane_y, f"{key}/a-rise"),
        wire(escape_a, lane_y, escape_b, lane_y, f"{key}/lane"),
        wire(escape_b, lane_y, escape_b, b[1], f"{key}/b-fall"),
        wire(escape_b, b[1], b[0], b[1], f"{key}/b-escape"),
    ]


def visible_routes(items: list[Component]) -> list[str]:
    by_ref = {item.ref: item for item in items}
    routes: list[str] = []
    # Controller-to-PCA control paths; power rails remain labelled multi-drop nets.
    for offset, (esp_pin, pca_pin) in enumerate((("L04", "H02"), ("L12", "H04"), ("L15", "H03"))):
        routes.extend(route_outer(
            stub_point(by_ref["U1"], esp_pin), stub_point(by_ref["U2"], pca_pin),
            68.58 - offset * 3.81, 259.08 + offset * 3.81, 165.10 + offset * 3.81,
            f"control/{esp_pin}",
        ))
    # Each PWM signal is visibly wired to its off-board servo. V+ and GND use labelled rails.
    for channel in range(8):
        routes.extend(route_via_x(
            stub_point(by_ref["U2"], f"CH{channel}-S"), stub_point(by_ref[f"M{channel + 1}"], "S"),
            335.28 + channel * 3.81, f"servo/{channel + 1}",
        ))
    routes.extend(route_via_x(stub_point(by_ref["U3"], "SPK+"), stub_point(by_ref["SP1"], "SPK+"), 320.04, "speaker/plus"))
    routes.extend(route_via_x(stub_point(by_ref["U3"], "SPK-"), stub_point(by_ref["SP1"], "SPK-"), 323.85, "speaker/minus"))
    return routes


def instance(comp: Component) -> tuple[str, list[str]]:
    name = f"Niulai:{comp.ref}_{comp.value}"
    half_height = max(5.08, (max(comp.left_count, len(comp.pins) - comp.left_count) - 1) * 1.27 + 2.54)
    sx = round(comp.x / 1.27) * 1.27
    sy = round(comp.y / 1.27) * 1.27
    pins = []
    connections = []
    for index, (pin_name, net) in enumerate(comp.pins, start=1):
        # KiCad symbol-library Y coordinates are mirrored when instantiated on
        # the schematic sheet: positive local Y appears above the instance
        # origin (smaller sheet Y). Keep wires/labels on the actual pin point.
        side, side_index, side_count = side_position(comp, index)
        y = sy - ((side_count + 1) / 2 - side_index) * 2.54
        x = sx - 15.24 if side == "left" else sx + 15.24
        number = pin_number(pin_name, index)
        pins.append(f'''\t\t(pin "{esc(number)}" (uuid "{sid(comp.ref, 'pin', number)}"))''')
        if net is None:
            connections.append(no_connect(x, y, f"{comp.ref}/{index}"))
        else:
            wire_end_x = x - 7.62 if side == "left" else x + 7.62
            connections.append(wire(x, y, wire_end_x, y, f"{comp.ref}/{index}/{net}"))
            connections.append(label(net, wire_end_x, y, f"{comp.ref}/{index}/{net}", 180 if side == "left" else 0))
    return f'''\t(symbol
\t\t(lib_id "{esc(name)}")
\t\t(at {sx:.3f} {sy:.3f} 0)
\t\t(unit 1)
\t\t(exclude_from_sim no)
\t\t(in_bom {'yes' if comp.in_bom else 'no'})
\t\t(on_board yes)
\t\t(dnp {'yes' if comp.dnp else 'no'})
\t\t(uuid "{sid(comp.ref, 'instance')}")
{prop('Reference', comp.ref, sx, sy - half_height - 3.81)}
{prop('Value', comp.value, sx, sy + half_height + 3.81)}
{prop('Footprint', '' if comp.footprint.startswith('FOOTPRINT_TBD') else comp.footprint, sx, sy, True)}
{prop('Datasheet', '', sx, sy, True)}
{prop('Description', comp.description, sx, sy, True)}
{chr(10).join(pins)}
\t\t(instances
\t\t\t(project "{PROJECT}"
\t\t\t\t(path "/{ROOT_UUID}" (reference "{comp.ref}") (unit 1))
\t\t\t)
\t\t)
\t)''', connections


def text_note(value: str, x: float, y: float, key: str, size: float = 1.5) -> str:
    return f'''\t(text "{esc(value)}"
\t\t(exclude_from_sim no)
\t\t(at {x:.3f} {y:.3f} 0)
\t\t(effects (font (size {size:.2f} {size:.2f}) (bold yes)) (justify left bottom))
\t\t(uuid "{sid('text', key)}")
\t)'''


def generate_schematic(items: list[Component]) -> str:
    lib_symbols = "\n".join(lib_symbol(item) for item in items)
    inst = []
    connections: list[str] = []
    for item in items:
        body, labels = instance(item)
        inst.append(body)
        connections.extend(labels)
    routes = visible_routes(items)
    notes = [
        text_note("POWER / PROTECTION -- J1 board side is XT30 MALE; source cables FEMALE", 18, 16, "power", 2.0),
        text_note("LM2596 INPUT CHECK: product spec VIN 3.2-40V and >=VOUT+1.5V; 6.0V-3.3V=2.7V PASS. Load-test at acceptance.", 18, 65, "lmcheck", 1.5),
        text_note("ESP32-S3 N16R8 candidate -- JP1 removable external-3V3 link; review USB/external power interaction", 18, 75, "esp", 1.8),
        text_note("U1 is one complete 2x22 module: L01/R01 start at antenna end. J3 exposes fly-wire and unused GPIO.", 18, 183, "gpio", 1.4),
        text_note("PCA9685 / AUDIO -- Option A servo V+ distribution; OE defaults HIGH via R1", 310, 65, "pca", 1.8),
        text_note("I2S clocks shared: GPIO39=BCLK, GPIO40=WS; GPIO41->amp DIN; GPIO47<-mic SD", 310, 174, "i2s", 1.4),
        text_note("FOOT INPUTS -- active LOW; 10k pull-up; optional 100nF DNP", 310, 188, "foot", 1.6),
        text_note("MPU6500/ST7789 are external Dupont-wired modules using J3; no dedicated Carrier footprints", 18, 275, "flywire", 1.5),
        text_note("REVIEW CANDIDATE -- FOOTPRINT_TBD fields must be bound and imported nets checked in EasyEDA Pro", 18, 330, "review", 1.7),
    ]
    return f'''(kicad_sch
\t(version 20240602)
\t(generator "niulai_generator")
\t(generator_version "0.2")
\t(uuid "{ROOT_UUID}")
\t(paper "A2")
\t(title_block
\t\t(title "Niulai Modular Carrier v0.2")
\t\t(date "2026-09-01")
\t\t(rev "Review Candidate 4 - physical module pinout")
\t\t(company "Niulai")
\t\t(comment 1 "Single XT30 input / 10A fuse / ESP32-S3 / PCA9685")
\t\t(comment 2 "NOT READY FOR PCB: exact footprints and physical module pinouts pending")
\t)
\t(lib_symbols
{lib_symbols}
\t)
{chr(10).join(notes)}
{chr(10).join(connections)}
{chr(10).join(routes)}
{chr(10).join(inst)}
\t(sheet_instances
\t\t(path "/" (page "1"))
\t)
)'''


def generate_csv(items: list[Component]) -> None:
    with (OUT / f"{PROJECT}_bom.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["Reference", "Value", "Footprint", "DNP", "Description"])
        for item in items:
            if item.in_bom:
                writer.writerow([item.ref, item.value, item.footprint, "YES" if item.dnp else "NO", item.description])
    with (OUT / f"{PROJECT}_netlist.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["Reference", "Pin", "Pin Name", "Net", "DNP"])
        for item in items:
            for index, (pin_name, net) in enumerate(item.pins, start=1):
                writer.writerow([item.ref, pin_number(pin_name, index), pin_name, net or "NC", "YES" if item.dnp else "NO"])


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    items = components()
    schematic = OUT / f"{PROJECT}.kicad_sch"
    schematic.write_text(generate_schematic(items), encoding="utf-8")
    symbol_entries = "\n".join(lib_symbol(item) for item in items).replace(
        '(symbol "Niulai:', '(symbol "'
    )
    (OUT / f"{PROJECT}.kicad_sym").write_text(
        f'(kicad_symbol_lib\n\t(version 20241209)\n\t(generator "niulai_generator")\n{symbol_entries}\n)\n',
        encoding="utf-8",
    )
    (OUT / "sym-lib-table").write_text(
        f'(sym_lib_table\n  (version 7)\n  (lib (name "Niulai")(type "KiCad")(uri "${{KIPRJMOD}}/{PROJECT}.kicad_sym")(options "")(descr "Project-local generated symbols"))\n)\n',
        encoding="utf-8",
    )
    footprint_libraries = (
        "Capacitor_SMD",
        "Capacitor_THT",
        "Connector_PinHeader_2.54mm",
        "Resistor_SMD",
        "TestPoint",
    )
    fp_lines = "\n".join(
        f'  (lib (name "{name}")(type "KiCad")(uri "${{KICAD10_FOOTPRINT_DIR}}/{name}.pretty")(options "")(descr ""))'
        for name in footprint_libraries
    )
    (OUT / "fp-lib-table").write_text(
        f"(fp_lib_table\n  (version 7)\n{fp_lines}\n)\n", encoding="utf-8"
    )
    project = {
        "board": {},
        "boards": [],
        "cvpcb": {},
        "erc": {},
        "libraries": {},
        "meta": {"filename": f"{PROJECT}.kicad_pro", "version": 1},
        "net_settings": {},
        "pcbnew": {},
        "schematic": {},
        "text_variables": {},
    }
    (OUT / f"{PROJECT}.kicad_pro").write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
    generate_csv(items)
    archive = OUT / f"{PROJECT}_easyeda_import.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        paths = [
            OUT / f"{PROJECT}.kicad_sch",
            OUT / f"{PROJECT}.kicad_pro",
            OUT / f"{PROJECT}.kicad_sym",
            OUT / f"{PROJECT}_bom.csv",
            OUT / f"{PROJECT}_netlist.csv",
            OUT / "sym-lib-table",
            OUT / "fp-lib-table",
        ]
        for path in paths:
            bundle.write(path, arcname=path.name)
    print(f"generated {schematic}")
    print(f"components: {len(items)}")
    print(f"archive: {archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
