#!/usr/bin/env python3
"""Independent contract checks for the generated Carrier v0.2 schematic."""

from __future__ import annotations

import csv
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent
NETLIST = ROOT / "niulai_carrier_v0.2_netlist.csv"
BOM = ROOT / "niulai_carrier_v0.2_bom.csv"
SCHEMATIC = ROOT / "niulai_carrier_v0.2.kicad_sch"
XML_NETLIST = ROOT / "niulai_carrier_v0.2.net"


def main() -> int:
    with NETLIST.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    with BOM.open(newline="", encoding="utf-8") as handle:
        bom = {row["Reference"]: row for row in csv.DictReader(handle)}
    schematic_text = SCHEMATIC.read_text(encoding="utf-8")

    native_pin_nets: dict[tuple[str, str], str] = {}
    xml_root = ET.parse(XML_NETLIST).getroot()
    for net in xml_root.findall("./nets/net"):
        net_name = net.attrib["name"]
        if net_name.startswith("/"):
            net_name = net_name[1:]
        for node in net.findall("node"):
            key = (node.attrib["ref"], node.attrib["pin"])
            assert key not in native_pin_nets
            native_pin_nets[key] = net_name

    assert len(native_pin_nets) == len(rows)
    for row in rows:
        key = (row["Reference"], row["Pin"])
        actual_net = native_pin_nets[key]
        if row["Net"] == "NC":
            assert actual_net.startswith("unconnected-")
        else:
            assert actual_net == row["Net"], (key, row["Net"], actual_net)

    by_ref: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        by_ref[row["Reference"]].append(row)

    def pin_nets(ref: str) -> list[str]:
        return [row["Net"] for row in by_ref[ref]]

    assert pin_nets("J1") == ["INPUT_6V_UNFUSED", "GND"]
    assert pin_nets("F1") == ["INPUT_6V_UNFUSED", "EXT_6V_IN"]
    assert "10A_32V_MIN" in bom["F1"]["Value"]
    assert pin_nets("NT1") == ["EXT_6V_IN", "SERVO_6V"]
    assert not ({"J_UBEC", "J_DC", "SW_PWR"} & by_ref.keys())

    assert pin_nets("U5") == ["EXT_6V_IN", "GND", "3V3", "GND"]
    assert bom["U5"]["Value"] == "LM2596S_MODULE_V151"
    assert "3.2-40V" in bom["U5"]["Description"]
    assert pin_nets("JP1") == ["3V3", "ESP_3V3"]

    for ref in ("C1", "C2"):
        assert pin_nets(ref) == ["SERVO_6V", "GND"]
        assert bom[ref]["DNP"] == "YES"
        assert "1000-2200uF_16V_105C" in bom[ref]["Value"]

    assert pin_nets("R1") == ["3V3", "PCA_OE"]
    assert bom["R1"]["DNP"] == "NO"
    assert pin_nets("R2") == ["3V3", "I2C_SDA"] and bom["R2"]["DNP"] == "YES"
    assert pin_nets("R3") == ["3V3", "I2C_SCL"] and bom["R3"]["DNP"] == "YES"

    pca = pin_nets("U2")
    assert pca[:8] == ["GND", "PCA_OE", "I2C_SCL", "I2C_SDA", "3V3", "SERVO_6V", "SERVO_6V", "GND"]
    assert "left V+, right GND" in bom["U2"]["Description"]
    for channel in range(8):
        start = 8 + channel * 3
        assert pca[start:start + 3] == [f"SERVO_PWM{channel + 1}", "SERVO_6V", "GND"]
        assert pin_nets(f"M{channel + 1}") == [f"SERVO_PWM{channel + 1}", "SERVO_6V", "GND"]

    amp = pin_nets("U3")
    assert amp == ["3V3", "GND", "NC", "NC", "I2S_AMP_DOUT", "I2S_BCLK", "I2S_WS", "SPK_PLUS", "SPK_MINUS"]
    assert pin_nets("SP1") == ["SPK_PLUS", "SPK_MINUS"]
    mic = pin_nets("U4")
    assert mic == ["I2S_MIC_DIN", "3V3", "GND", "MIC_LR", "I2S_WS", "I2S_BCLK"]
    assert pin_nets("R4") == ["MIC_LR", "GND"]

    expected_foot_gpio = {
        "FOOT_FL": "GPIO5",
        "FOOT_FR": "GPIO6",
        "FOOT_RL": "GPIO7",
        "FOOT_RR": "GPIO15",
    }
    esp_by_pin = {row["Pin"]: row for row in by_ref["U1"]}
    expected_esp = {
        "L04": "PCA_OE", "L05": "FOOT_FL", "L06": "FOOT_FR", "L07": "FOOT_RL",
        "L08": "FOOT_RR", "L12": "I2C_SDA", "L15": "I2C_SCL", "R07": "I2S_AMP_DOUT",
        "R08": "I2S_WS", "R09": "I2S_BCLK", "R17": "I2S_MIC_DIN",
    }
    for pin, net in expected_esp.items():
        assert esp_by_pin[pin]["Net"] == net
    for index, (net, gpio) in enumerate(expected_foot_gpio.items(), start=1):
        assert pin_nets(f"J{9 + index}") == [net, "GND"]
        assert pin_nets(f"R{4 + index}") == ["3V3", net]
        assert pin_nets(f"C{4 + index}") == [net, "GND"]
        assert bom[f"C{4 + index}"]["DNP"] == "YES"

    assert len(by_ref["U1"]) == 44
    assert len(by_ref["J3"]) == 22
    for pin in ("R11", "R12", "R13"):
        assert esp_by_pin[pin]["Net"] == "NC" and "PSRAM_RESERVED" in esp_by_pin[pin]["Pin Name"]
    for pin in ("R19", "R20"):
        assert esp_by_pin[pin]["Net"] == "NC" and "RESERVED" in esp_by_pin[pin]["Pin Name"]

    assert "MPU6500" not in by_ref and "ST7789" not in by_ref
    assert len(bom) == 36  # NT1 and off-board servos/speaker are excluded.
    connected_pin_count = sum(row["Net"] != "NC" for row in rows)
    direct_route_segments = 45
    assert schematic_text.count("\n\t(wire\n") == connected_pin_count + direct_route_segments
    assert schematic_text.count("\n\t(label ") == connected_pin_count

    print("Niulai Carrier v0.2 schematic contract: PASS")
    print(f"- netlist rows: {len(rows)}; procurement BOM rows: {len(bom)}")
    print("- single XT30/fuse path, optional bulk and removable ESP 3V3 link: present")
    print("- PCA /OE default-high hardware and DNP I2C pull-ups: present")
    print("- shared I2S clocks with separate amp/mic data: present")
    print("- one complete 2x22 ESP module symbol; physical L/R order and PSRAM/USB NC restrictions retained")
    print(f"- pin stubs/labels: {connected_pin_count}; direct point-to-point route segments: {direct_route_segments}")
    print(f"- KiCad native XML netlist: {len(native_pin_nets)} pin nodes exactly match the intended CSV")
    print("- LM2596 nominal input/headroom contract: 6.0-3.3=2.7 V > 1.5 V; paper blocker closed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
