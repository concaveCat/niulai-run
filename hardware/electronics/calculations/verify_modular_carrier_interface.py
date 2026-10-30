#!/usr/bin/env python3
"""Static completeness check for the modular Carrier interface table."""

from __future__ import annotations

import csv
from collections import defaultdict
from pathlib import Path


TABLE = Path(__file__).parents[1] / "MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv"
ALLOWED_STATUSES = {
    "FROZEN",
    "CANDIDATE",
    "BLOCKED_MPN",
    "BLOCKED_BOARD",
    "BLOCKED_PHYSICAL",
    "BLOCKED_INTERFACE",
    "BLOCKED_MEASUREMENT",
    "BLOCKED_HIGH_RISK",
    "ASSUMED_UNVERIFIED",
    "OPERATOR_CONFIRMED",
}
REQUIRED_COLUMNS = {
    "Module",
    "Pin",
    "Signal",
    "Direction",
    "Voltage Domain",
    "ESP32 GPIO",
    "Connector",
    "Pull",
    "Status",
    "Notes",
}


def main() -> int:
    with TABLE.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        assert reader.fieldnames is not None
        assert set(reader.fieldnames) == REQUIRED_COLUMNS
        rows = list(reader)

    assert rows
    assert all(row["Status"] in ALLOWED_STATUSES for row in rows)
    assert all(row["Module"] and row["Pin"] and row["Signal"] for row in rows)

    modules = {row["Module"] for row in rows}
    required_modules = {
        "J_PWR",
        "F_MAIN",
        "POWER_DIST",
        "C_BULK1",
        "C_BULK2",
        "GPIO_BREAKOUT",
        "LM2596S",
        "ESP32",
        "PCA9685",
        "MPU6500",
        "ST7789",
        "MAX98357A",
        "MS4030",
        "FOOT_FL",
        "FOOT_FR",
        "FOOT_RL",
        "FOOT_RR",
        "TESTPOINT",
    }
    assert required_modules <= modules

    signals_by_gpio: dict[str, set[str]] = defaultdict(set)
    for row in rows:
        gpio = row["ESP32 GPIO"]
        if gpio and gpio != "TBD":
            signals_by_gpio[gpio].add(row["Signal"])
    conflicts = {
        gpio: signals for gpio, signals in signals_by_gpio.items() if len(signals) > 1
    }
    assert not conflicts, f"GPIO conflicts: {conflicts}"

    servo_rows = [row for row in rows if row["Signal"].startswith("SERVO_PWM")]
    assert {row["Signal"] for row in servo_rows} == {
        f"SERVO_PWM{index}" for index in range(1, 9)
    }
    assert all(row["Status"] == "ASSUMED_UNVERIFIED" for row in servo_rows)

    foot_rows = [row for row in rows if row["Module"].startswith("FOOT_")]
    assert len(foot_rows) == 4
    assert all("10k_to_3V3" in row["Pull"] for row in foot_rows)

    oe_rows = [row for row in rows if row["Signal"] == "PCA_OE"]
    assert oe_rows
    assert all("10k_to_3V3" in row["Pull"] for row in oe_rows)
    assert {row["ESP32 GPIO"] for row in oe_rows} == {"GPIO4"}

    assert not ({"J_UBEC", "J_DC", "SW_PWR"} & modules)

    main_input = {row["Pin"]: row for row in rows if row["Module"] == "J_PWR"}
    assert main_input["POS"]["Signal"] == "INPUT_6V_UNFUSED"
    assert main_input["NEG"]["Signal"] == "GND"

    fuse = {row["Pin"]: row for row in rows if row["Module"] == "F_MAIN"}
    assert fuse["IN"]["Signal"] == "INPUT_6V_UNFUSED"
    assert fuse["OUT"]["Signal"] == "EXT_6V_IN"
    assert all(row["Status"] == "FROZEN" for row in fuse.values())

    bulk_rows = [row for row in rows if row["Module"] in {"C_BULK1", "C_BULK2"}]
    assert len(bulk_rows) == 4
    assert {row["Signal"] for row in bulk_rows} == {"SERVO_6V", "GND"}
    assert all(row["Status"] == "FROZEN" for row in bulk_rows)

    flying_wire_rows = [
        row for row in rows if row["Module"] in {"MPU6500", "ST7789"}
    ]
    assert flying_wire_rows
    assert all(row["Connector"] == "J_GPIO_BREAKOUT" for row in flying_wire_rows)
    assert not any(row["Connector"] in {"J_IMU", "J_DISPLAY"} for row in rows)

    breakout = [row for row in rows if row["Module"] == "GPIO_BREAKOUT"]
    assert {row["Signal"] for row in breakout} == {
        "ESP32_UNUSED_GPIO",
        "3V3",
        "GND",
    }

    blocked = [row for row in rows if row["Status"].startswith("BLOCKED_")]
    assumptions = [row for row in rows if row["Status"] == "ASSUMED_UNVERIFIED"]
    print("Modular Carrier interface table: PASS")
    print(f"- rows: {len(rows)}")
    print(f"- assigned ESP32 GPIOs: {len(signals_by_gpio)}; conflicts: 0")
    print("- servo PWM rows: 8; foot inputs: 4; PCA /OE default-high rule: present")
    print("- power nets: single XT30 J_PWR -> F_MAIN -> EXT_6V_IN")
    print("- MPU6500/ST7789: generic GPIO breakout and Dupont flying wires")
    print("- all final unused ESP32 GPIOs: breakout contract present")
    print(f"- unresolved rows intentionally retained: {len(blocked)}")
    print(f"- Operator-accepted unverified assumption rows: {len(assumptions)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
