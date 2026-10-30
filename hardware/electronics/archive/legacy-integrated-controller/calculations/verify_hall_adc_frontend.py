#!/usr/bin/env python3
"""Verify the selected HAL49ESO-to-ESP32 ADC divider and filter.

This establishes electrical range and bandwidth. It does not predict magnetic
transfer accuracy, which remains a prototype validation risk.
"""

from __future__ import annotations

from math import pi


def main() -> int:
    errors: list[str] = []

    rail_max_v = 3.6
    adc_effective_max_v = 2.9
    r_top_nom_ohm = 27_400.0
    r_bottom_nom_ohm = 100_000.0
    resistor_tolerance = 0.01
    filter_cap_nom_f = 100e-9
    capacitor_tolerance = 0.10
    hall_required_load_min_ohm = 10_000.0

    r_top_min = r_top_nom_ohm * (1.0 - resistor_tolerance)
    r_top_max = r_top_nom_ohm * (1.0 + resistor_tolerance)
    r_bottom_min = r_bottom_nom_ohm * (1.0 - resistor_tolerance)
    r_bottom_max = r_bottom_nom_ohm * (1.0 + resistor_tolerance)

    ratio_nom = r_bottom_nom_ohm / (r_top_nom_ohm + r_bottom_nom_ohm)
    ratio_max = r_bottom_max / (r_top_min + r_bottom_max)
    ratio_min = r_bottom_min / (r_top_max + r_bottom_min)
    adc_input_worst_v = rail_max_v * ratio_max
    adc_headroom_v = adc_effective_max_v - adc_input_worst_v

    hall_load_min_ohm = r_top_min + r_bottom_min
    if hall_load_min_ohm < hall_required_load_min_ohm:
        errors.append("divider loads the Hall output below its 10 kohm test condition")
    if adc_headroom_v <= 0:
        errors.append("worst divider output exceeds ESP32 ATTEN3 effective range")

    r_thevenin_nom_ohm = (
        r_top_nom_ohm * r_bottom_nom_ohm
        / (r_top_nom_ohm + r_bottom_nom_ohm)
    )
    cap_max_f = filter_cap_nom_f * (1.0 + capacitor_tolerance)
    cap_min_f = filter_cap_nom_f * (1.0 - capacitor_tolerance)
    cutoff_nom_hz = 1.0 / (2.0 * pi * r_thevenin_nom_ohm * filter_cap_nom_f)
    cutoff_min_hz = 1.0 / (2.0 * pi * r_thevenin_nom_ohm * cap_max_f)
    cutoff_max_hz = 1.0 / (2.0 * pi * r_thevenin_nom_ohm * cap_min_f)

    if errors:
        print("Hall ADC front end: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Hall ADC front end: PASS")
    print(
        f"- divider: 27.4 kohm / 100 kohm, both +/-1%; "
        f"nominal ratio {ratio_nom:.5f}"
    )
    print(
        f"- divider ratio tolerance: {ratio_min:.5f} to {ratio_max:.5f}"
    )
    print(
        f"- worst ADC voltage for Hall output bounded by {rail_max_v:.1f} V: "
        f"{adc_input_worst_v:.3f} V"
    )
    print(
        f"- headroom below ESP32 ATTEN3 2.9 V effective limit: "
        f"{adc_headroom_v * 1000:.1f} mV"
    )
    print(f"- minimum Hall DC load: {hall_load_min_ohm / 1000:.1f} kohm")
    print(
        f"- 100 nF +/-10% RC cutoff: nominal {cutoff_nom_hz:.1f} Hz, "
        f"range {cutoff_min_hz:.1f} to {cutoff_max_hz:.1f} Hz"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
