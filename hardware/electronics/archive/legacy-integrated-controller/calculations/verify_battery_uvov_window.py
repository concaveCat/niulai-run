#!/usr/bin/env python3
"""Worst-case LTC4368 UV/OV divider check for the frozen 3S range.

The equations solve the three-resistor network in LTC4368 Rev. C Figure 4:
VIN -- R3 -- UV -- R2 -- OV -- R1 -- GND.
"""

from __future__ import annotations

from itertools import product


R1_NOM = 67_800.0
R2_NOM = 32_400.0
R3_NOM = 1_660_000.0
R_TOL = 0.001
PIN_LEAK_MAX_A = 10e-9

THRESHOLD_FALL_MIN_V = 0.4925
THRESHOLD_FALL_MAX_V = 0.5075
HYST_MIN_V = 0.020
HYST_MAX_V = 0.032

FROZEN_VBAT_MIN_V = 9.0
FROZEN_VBAT_MAX_V = 12.6


def vin_for_uv(
    r1: float,
    r2: float,
    r3: float,
    uv_v: float,
    i_uv_a: float,
    i_ov_a: float,
) -> float:
    """Solve VIN with UV fixed at its comparator threshold."""
    ov_v = (uv_v / r2 - i_ov_a) / (1.0 / r2 + 1.0 / r1)
    return uv_v + r3 * ((uv_v - ov_v) / r2 + i_uv_a)


def vin_for_ov(
    r1: float,
    r2: float,
    r3: float,
    ov_v: float,
    i_uv_a: float,
    i_ov_a: float,
) -> float:
    """Solve VIN with OV fixed at its comparator threshold."""
    uv_v = ov_v + r2 * (ov_v / r1 + i_ov_a)
    return uv_v + r3 * ((uv_v - ov_v) / r2 + i_uv_a)


def corners(kind: str) -> list[float]:
    results: list[float] = []
    if kind in {"uv_falling", "ov_rising"}:
        thresholds = (THRESHOLD_FALL_MIN_V, THRESHOLD_FALL_MAX_V)
    elif kind == "uv_rising":
        thresholds = (
            THRESHOLD_FALL_MIN_V + HYST_MIN_V,
            THRESHOLD_FALL_MAX_V + HYST_MAX_V,
        )
    elif kind == "ov_falling":
        thresholds = (
            THRESHOLD_FALL_MIN_V - HYST_MAX_V,
            THRESHOLD_FALL_MAX_V - HYST_MIN_V,
        )
    else:
        raise ValueError(kind)

    for tolerances in product((-R_TOL, R_TOL), repeat=3):
        r1, r2, r3 = (
            nominal * (1.0 + tolerance)
            for nominal, tolerance in zip(
                (R1_NOM, R2_NOM, R3_NOM), tolerances
            )
        )
        for threshold_v in thresholds:
            for i_uv_a, i_ov_a in product(
                (-PIN_LEAK_MAX_A, PIN_LEAK_MAX_A), repeat=2
            ):
                if kind.startswith("uv"):
                    vin_v = vin_for_uv(
                        r1, r2, r3, threshold_v, i_uv_a, i_ov_a
                    )
                else:
                    vin_v = vin_for_ov(
                        r1, r2, r3, threshold_v, i_uv_a, i_ov_a
                    )
                results.append(vin_v)
    return results


def main() -> int:
    ranges = {kind: corners(kind) for kind in (
        "uv_falling", "uv_rising", "ov_rising", "ov_falling"
    )}
    errors: list[str] = []

    if max(ranges["uv_falling"]) >= FROZEN_VBAT_MIN_V:
        errors.append("UV shutdown can occur inside the frozen 9.0 V lower bound")
    if min(ranges["ov_rising"]) <= FROZEN_VBAT_MAX_V:
        errors.append("OV trip can occur inside the frozen 12.6 V upper bound")

    if errors:
        print("Battery UV/OV window: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Battery UV/OV window: PASS")
    print(
        "- selected divider: R1(bottom)=67.8 kohm, R2=32.4 kohm, "
        "R3(top)=1.66 Mohm; all +/-0.1%"
    )
    print("- corners include comparator limits, hysteresis limits, resistor tolerance,")
    print("  and independent +/-10 nA UV/OV leakage")
    for name, values in ranges.items():
        print(
            f"- {name.replace('_', ' ')}: "
            f"{min(values):.3f} to {max(values):.3f} V"
        )
    print(
        f"- frozen valid range: {FROZEN_VBAT_MIN_V:.1f} to "
        f"{FROZEN_VBAT_MAX_V:.1f} V"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
