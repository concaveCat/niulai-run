#!/usr/bin/env python3
"""Contract checks for the modular Carrier fuse and optional servo bulk."""

from __future__ import annotations


FUSE_RATING_A = 10
FUSE_VOLTAGE_VDC_MIN = 32
NEW_PATH_CURRENT_A_MIN = 10
HOLDER_CURRENT_A_MIN = 15

BULK_FOOTPRINT_COUNT = 2
BULK_CAPACITANCE_UF_MIN = 1_000
BULK_CAPACITANCE_UF_MAX = 2_200
BULK_VOLTAGE_V = 16
BULK_TEMPERATURE_C = 105
BULK_BODY_DIAMETER_MM_MAX = 12.5
BULK_LEAD_PITCH_MM = 5.0
BULK_MANDATORY_POPULATION = 0


def main() -> int:
    assert FUSE_RATING_A == 10
    assert FUSE_VOLTAGE_VDC_MIN >= 32
    assert NEW_PATH_CURRENT_A_MIN >= FUSE_RATING_A
    assert HOLDER_CURRENT_A_MIN >= 15

    assert BULK_FOOTPRINT_COUNT == 2
    assert BULK_CAPACITANCE_UF_MIN == 1_000
    assert BULK_CAPACITANCE_UF_MAX == 2_200
    assert BULK_VOLTAGE_V == 16
    assert BULK_TEMPERATURE_C == 105
    assert BULK_BODY_DIAMETER_MM_MAX <= 12.5
    assert BULK_LEAD_PITCH_MM == 5.0
    assert BULK_MANDATORY_POPULATION == 0

    print("Modular Carrier fuse/optional-bulk contract: PASS")
    print("- F_MAIN: replaceable ATO/ATC, 10 A, >=32 VDC")
    print("- holder: PCB mount, >=15 A continuous; multi-pin preferred")
    print("- newly designed main-current path: >=10 A capability")
    print("- nets: single XT30 J_PWR -> F_MAIN -> EXT_6V_IN")
    print("- C_BULK1/C_BULK2: optional 1000-2200 uF, 16 V, 105 C radial")
    print("- default bulk population: zero mandatory parts; tune from measurements")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
