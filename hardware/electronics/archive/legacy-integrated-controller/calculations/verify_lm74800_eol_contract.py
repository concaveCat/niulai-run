#!/usr/bin/env python3
"""Static checks for the LM74800 per-PCBA reverse-leakage EOL contract.

This verifies the accepted test matrix and guardbands.  It does not predict
unbounded high-temperature device leakage or replace measurements.
"""

from __future__ import annotations


FINAL_LIMIT_UA = 100.0
ROOM_LIMIT_UA = 50.0
HOT_LIMIT_UA = 75.0
U95_TARGET_UA = 5.0
BLANK_FIXTURE_LIMIT_UA = 2.0
CURRENT_RESOLUTION_UA = 0.1
COMPLIANCE_MA = 25.0

HD303_IDSS_MAX_UA_25C = 1.0
HD303_IGSS_MAX_UA_25C = 0.1
HD303_COUNT = 2

TEMPERATURE_POINTS_C = (25.0, 40.0)
BATTERY_POINTS_V = (0.0, 9.0, 12.6)
SMALL_REVERSE_DELTA_V = 0.25

REQUIRED_MATRIX_IDS = {
    "N1",
    "N2",
    "O1",
    "O2",
    "E1",
    "E2",
    "F1",
    "F2",
    "Z1",
    "Z2",
    "H1",
    "H2",
}


def main() -> int:
    hd303_conditional_sum = HD303_COUNT * (
        HD303_IDSS_MAX_UA_25C + HD303_IGSS_MAX_UA_25C
    )

    assert hd303_conditional_sum == 2.2
    assert ROOM_LIMIT_UA < HOT_LIMIT_UA < FINAL_LIMIT_UA
    assert U95_TARGET_UA <= FINAL_LIMIT_UA - HOT_LIMIT_UA
    assert BLANK_FIXTURE_LIMIT_UA < U95_TARGET_UA
    assert CURRENT_RESOLUTION_UA <= 0.1
    assert COMPLIANCE_MA <= 25.0
    assert TEMPERATURE_POINTS_C == (25.0, 40.0)
    assert BATTERY_POINTS_V == (0.0, 9.0, 12.6)
    assert SMALL_REVERSE_DELTA_V > 0.0
    assert len(REQUIRED_MATRIX_IDS) == 12

    print("LM74800 per-PCBA reverse-leakage EOL contract: PASS")
    print(
        "- HD303 25 C conditional maximum sum: "
        f"{hd303_conditional_sum:.1f} uA; not an assembled-board limit"
    )
    print(
        f"- every first-batch PCBA: {TEMPERATURE_POINTS_C[0]:.0f} C <= "
        f"{ROOM_LIMIT_UA:.0f} uA and {TEMPERATURE_POINTS_C[1]:.0f} C <= "
        f"{HOT_LIMIT_UA:.0f} uA, both including U95"
    )
    print(f"- functional ceiling at every tested state: {FINAL_LIMIT_UA:.0f} uA")
    print(
        f"- instrument targets: resolution <= {CURRENT_RESOLUTION_UA:.1f} uA, "
        f"U95 <= {U95_TARGET_UA:.0f} uA, blank <= {BLANK_FIXTURE_LIMIT_UA:.0f} uA"
    )
    print(f"- mandatory state rows: {', '.join(sorted(REQUIRED_MATRIX_IDS))}")
    print("- test data are still required; this script does not close PVR-017")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
