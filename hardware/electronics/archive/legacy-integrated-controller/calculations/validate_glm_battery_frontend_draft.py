#!/usr/bin/env python3
"""Independent arithmetic checks for glm_battery_frontend_draft.md.

This validates arithmetic only and prints corrected design-screening values.
It deliberately does not declare the candidate topology accepted.
"""

from __future__ import annotations


VCL_MIN_V = 48e-3
VCL_TYP_V = 55e-3
VCL_MAX_V = 65e-3
RSENSE_OHM = 6.2e-3
RSENSE_TOL = 0.01

VBAT_MAX_V = 12.6
PLIM_NOM_W = 14.0
PWR_THRESHOLD_MIN_SCALE = 19.0 / 25.0
PWR_THRESHOLD_MAX_SCALE = 31.0 / 25.0

CTIMER_F = 100e-9
CTIMER_TOL = 0.10
VTIMER_MIN_V = 3.2
VTIMER_TYP_V = 3.6
VTIMER_MAX_V = 4.0
ITIMER_MIN_A = 45e-6
ITIMER_TYP_A = 70e-6
ITIMER_MAX_A = 95e-6

RDS_QREV_25C_MAX_OHM = 2.9e-3  # VGS=4.5 V
RDS_QPASS_25C_MAX_OHM = 1.5e-3  # VGS=10 V
RDS_TEMP_MULTIPLIER_SCREEN = 1.5
RTH_JA_MAX_C_PER_W = 65.0
TA_MAX_C = 40.0

FUSE_COLD_R_TYP_OHM = 6.8e-3
FUSE_DERATING_40C_GRAPH = 0.97


def close(actual: float, expected: float, tolerance: float = 1e-9) -> None:
    if abs(actual - expected) > tolerance:
        raise AssertionError(f"{actual} != {expected}")


def main() -> int:
    # Independently reproduce the draft's key arithmetic.
    i_nom = VCL_TYP_V / RSENSE_OHM
    i_min = VCL_MIN_V / (RSENSE_OHM * (1.0 + RSENSE_TOL))
    i_max = VCL_MAX_V / (RSENSE_OHM * (1.0 - RSENSE_TOL))
    close(i_nom, 8.870967741935484)
    close(i_min, 7.665282657297988)
    close(i_max, 10.589768654284784)

    # LMX5069 Eq. 13 and Eq. 14, using the datasheet's nominal model.
    plim_min_recommended = 5e-3 * VBAT_MAX_V / RSENSE_OHM
    rpwr_nom_ohm = 180_000.0 * RSENSE_OHM * (
        PLIM_NOM_W - 1e-3 * VBAT_MAX_V / RSENSE_OHM
    )
    plim_corner_min = PLIM_NOM_W * PWR_THRESHOLD_MIN_SCALE
    plim_corner_max = PLIM_NOM_W * PWR_THRESHOLD_MAX_SCALE

    # TIMER charge interval corners. A 100 nF nominal part keeps the
    # maximum first fault pulse below the JMSL's published 10 ms SOA curve.
    tfault_min_s = CTIMER_F * (1.0 - CTIMER_TOL) * VTIMER_MIN_V / ITIMER_MAX_A
    tfault_typ_s = CTIMER_F * VTIMER_TYP_V / ITIMER_TYP_A
    tfault_max_s = CTIMER_F * (1.0 + CTIMER_TOL) * VTIMER_MAX_V / ITIMER_MIN_A

    # Worst fully-enhanced conduction screening at 100 C. The 1.5 multiplier
    # is a conservative graph read, not a guaranteed temperature coefficient.
    qrev_r_hot = RDS_QREV_25C_MAX_OHM * RDS_TEMP_MULTIPLIER_SCREEN
    qpass_r_hot = RDS_QPASS_25C_MAX_OHM * RDS_TEMP_MULTIPLIER_SCREEN

    print("GLM battery-front-end draft: ARITHMETIC PASS / DESIGN CONCLUSION FAIL")
    print(f"RSENSE 6.2 mOhm +/-1%: ICL={i_min:.3f}/{i_nom:.3f}/{i_max:.3f} A min/typ/max")
    print(f"LMX minimum recommended PLIM at 12.6 V: {plim_min_recommended:.3f} W")
    print(f"PLIM 14 W nominal -> RPWR={rpwr_nom_ohm / 1e3:.3f} kohm nominal")
    print(f"First-order PWR-threshold screening: {plim_corner_min:.2f}..{plim_corner_max:.2f} W")
    print(
        "CTIMER 100 nF +/-10%: fault pulse "
        f"{tfault_min_s * 1e3:.3f}/{tfault_typ_s * 1e3:.3f}/"
        f"{tfault_max_s * 1e3:.3f} ms min/typ/max"
    )
    for current_a in (6.0, 7.5, 10.0):
        qrev_p = current_a**2 * qrev_r_hot
        qpass_p = current_a**2 * qpass_r_hot
        print(
            f"{current_a:.1f} A enhanced: QREV={qrev_p:.4f} W, "
            f"QPASS={qpass_p:.4f} W"
        )
    qrev_6a_p = 6.0**2 * qrev_r_hot
    qpass_6a_p = 6.0**2 * qpass_r_hot
    print(
        f"6 A RthetaJA screen at TA=40 C: QREV TJ={TA_MAX_C + qrev_6a_p * RTH_JA_MAX_C_PER_W:.1f} C, "
        f"QPASS TJ={TA_MAX_C + qpass_6a_p * RTH_JA_MAX_C_PER_W:.1f} C"
    )
    print(
        f"Fuse: 8 A is {8.0 / (10.0 * FUSE_DERATING_40C_GRAPH):.3f}x of graph-derated rating; "
        f"6 A cold-resistance loss is {6.0**2 * FUSE_COLD_R_TYP_OHM:.3f} W typical"
    )
    print("Unclosed: exact TVS, guaranteed PLIM corners, startup capacitance, repeated-fault latch policy, reverse-port proof.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
