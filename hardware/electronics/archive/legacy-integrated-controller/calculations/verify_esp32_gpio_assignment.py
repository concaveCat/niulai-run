#!/usr/bin/env python3
"""Verify the frozen ESP32-S3-WROOM-1-N16R8 GPIO allocation.

This is a table-consistency check, not a schematic ERC or firmware pin test.
It intentionally uses only the Python standard library.
"""

from __future__ import annotations

import csv
from pathlib import Path


HERE = Path(__file__).resolve().parent
CSV_PATH = HERE / "esp32_gpio_assignment.csv"

MODULE_GPIO = set(range(0, 22)) | set(range(35, 49))
OCTAL_PSRAM_GPIO = {35, 36, 37}
EXPANSION_GPIO = {1, 2, 7, 8, 9, 10, 13, 15, 38, 39, 40, 42, 45, 47, 48}
HALL_SHARED_GPIO = {7, 8, 9}
FREE_GPIO = EXPANSION_GPIO - HALL_SHARED_GPIO
RECOVERY_GPIO = {0, 43, 44}

REQUIRED_FUNCTION = {
    0: "BOOT_N",
    3: "TCA_RESET_N",
    4: "VBAT_ADC",
    5: "SERVO_CURRENT_ADC",
    6: "HALL1_ADC",
    11: "I2C_SDA",
    12: "I2C_SCL",
    14: "TCA_INT_N",
    16: "I2S_BCLK",
    17: "I2S_LRCLK",
    18: "I2S_DOUT",
    19: "USB_DATA_DN",
    20: "USB_DATA_DP",
    21: "PDM_DATA",
    41: "WS2812_DIN",
    43: "UART0_TX_RECOVERY",
    44: "UART0_RX_RECOVERY",
    46: "PDM_CLK",
}


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def main() -> int:
    rows = list(csv.DictReader(CSV_PATH.open(encoding="utf-8", newline="")))
    errors: list[str] = []
    by_gpio: dict[int, dict[str, str]] = {}

    required_columns = {
        "gpio",
        "function",
        "class",
        "connector_access",
        "assembly",
        "boot_or_electrical_constraint",
    }
    require(
        set(rows[0]) == required_columns if rows else False,
        f"CSV columns must be exactly {sorted(required_columns)}",
        errors,
    )

    for line_number, row in enumerate(rows, start=2):
        try:
            gpio = int(row["gpio"])
        except (KeyError, ValueError):
            errors.append(f"line {line_number}: invalid GPIO number")
            continue
        require(gpio not in by_gpio, f"GPIO{gpio} is assigned more than once", errors)
        by_gpio[gpio] = row

    actual_gpio = set(by_gpio)
    require(
        actual_gpio == MODULE_GPIO,
        f"module GPIO coverage mismatch: missing={sorted(MODULE_GPIO - actual_gpio)}, "
        f"extra={sorted(actual_gpio - MODULE_GPIO)}",
        errors,
    )

    reserved = {
        gpio for gpio, row in by_gpio.items() if row["class"] == "RESERVED_INTERNAL"
    }
    require(
        reserved == OCTAL_PSRAM_GPIO,
        f"N16R8 reserved GPIO must be {sorted(OCTAL_PSRAM_GPIO)}, "
        f"got {sorted(reserved)}",
        errors,
    )

    exported = {
        gpio for gpio, row in by_gpio.items() if row["connector_access"] == "HEADER"
    }
    require(
        exported == EXPANSION_GPIO,
        "header GPIO must exactly match the 15 frozen expansion pins; "
        f"got {sorted(exported)}",
        errors,
    )

    hall_shared = {
        gpio for gpio, row in by_gpio.items() if row["class"] == "HALL_SHARED"
    }
    require(
        hall_shared == HALL_SHARED_GPIO,
        f"HALL_SHARED must be GPIO7/8/9, got {sorted(hall_shared)}",
        errors,
    )

    free = {
        gpio
        for gpio, row in by_gpio.items()
        if row["class"] in {"FREE", "FREE_STRAP"}
    }
    require(
        free == FREE_GPIO,
        f"unconditional/free-strap set mismatch: got {sorted(free)}",
        errors,
    )

    recovery = {
        gpio for gpio, row in by_gpio.items() if row["class"] == "RECOVERY"
    }
    require(
        recovery == RECOVERY_GPIO,
        f"recovery GPIO must be {sorted(RECOVERY_GPIO)}, got {sorted(recovery)}",
        errors,
    )

    for gpio, expected_function in REQUIRED_FUNCTION.items():
        actual = by_gpio.get(gpio, {}).get("function")
        require(
            actual == expected_function,
            f"GPIO{gpio} must be {expected_function}, got {actual!r}",
            errors,
        )

    strap_expected = {0, 3, 45, 46}
    for gpio in strap_expected:
        constraint = by_gpio[gpio]["boot_or_electrical_constraint"].lower()
        require(
            "reset" in constraint or "strap" in constraint or "boot" in constraint,
            f"GPIO{gpio} lacks an explicit reset/strap constraint",
            errors,
        )

    if errors:
        print("ESP32 GPIO assignment: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    runtime_gpio = MODULE_GPIO - OCTAL_PSRAM_GPIO
    onboard_or_recovery = runtime_gpio - EXPANSION_GPIO
    print("ESP32 GPIO assignment: PASS")
    print(f"- module-exposed GPIO rows: {len(MODULE_GPIO)}")
    print(f"- N16R8 Octal-PSRAM-reserved GPIO: {len(OCTAL_PSRAM_GPIO)}")
    print(f"- board-usable GPIO: {len(runtime_gpio)}")
    print(
        f"- frozen expansion GPIO: {len(EXPANSION_GPIO)} "
        f"({len(FREE_GPIO)} free/free-strap + "
        f"{len(HALL_SHARED_GPIO)} HALL_SHARED)"
    )
    print(f"- onboard/recovery GPIO: {len(onboard_or_recovery)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
