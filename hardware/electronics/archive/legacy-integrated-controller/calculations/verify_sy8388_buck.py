#!/usr/bin/env python3
"""Reproducible first-pass sizing for the SY8388ARHC 6 V servo buck.

This is a circuit-design screening tool, not an ERC or prototype proof.  Exact
L/C MPNs, DC-bias curves, DCR, efficiency and PCB thermal spreading remain
inputs to the next review pass.
"""

from __future__ import annotations

import math


VIN_MIN = 9.0
VIN_MAX = 12.6
VOUT_TARGET = 6.0
VREF_NOM = 0.600
VREF_MIN = 0.594
VREF_MAX = 0.606
R_TOP_NOM = 100_000.0
R_BOTTOM_NOM = 11_100.0
R_TOL = 0.001

FSW_MIN = 510e3
FSW_NOM = 600e3
L_NOM = 2.7e-6
ALT_L_NOM = 2.2e-6
L_TOL = 0.20
VOUT_MAX_CURRENT = 8.0
VOUT_CONT_CURRENT = 6.0

TOP_RDS = 20e-3
BOTTOM_RDS = 10e-3
THETA_JA = 33.0
TA_MAX = 40.0
TJ_RECOMMENDED_MAX = 125.0

CIN_EFFECTIVE_MIN = 40e-6
COUT_EFFECTIVE_MIN = 330e-6
COUT_ESR_MAX = 10e-3


def feedback_voltage(vref: float, rtop: float, rbottom: float) -> float:
    return vref * (1.0 + rtop / rbottom)


def duty(vin: float) -> float:
    return VOUT_TARGET / vin


def ripple_current(vin: float, inductance: float, fsw: float) -> float:
    return VOUT_TARGET * (vin - VOUT_TARGET) / (vin * fsw * inductance)


def rms_inductor_current(iout: float, ripple: float) -> float:
    return math.sqrt(iout**2 + ripple**2 / 12.0)


def mosfet_conduction_loss(vin: float, iout: float) -> float:
    d = duty(vin)
    return iout**2 * (d * TOP_RDS + (1.0 - d) * BOTTOM_RDS)


def main() -> int:
    vout_nom = feedback_voltage(VREF_NOM, R_TOP_NOM, R_BOTTOM_NOM)
    vout_min = feedback_voltage(
        VREF_MIN, R_TOP_NOM * (1.0 - R_TOL), R_BOTTOM_NOM * (1.0 + R_TOL)
    )
    vout_max = feedback_voltage(
        VREF_MAX, R_TOP_NOM * (1.0 + R_TOL), R_BOTTOM_NOM * (1.0 - R_TOL)
    )
    assert 5.9 < vout_nom < 6.1
    assert vout_min < vout_nom < vout_max

    l_min = L_NOM * (1.0 - L_TOL)
    l_max = L_NOM * (1.0 + L_TOL)
    ripple_worst = ripple_current(VIN_MAX, l_min, FSW_MIN)
    alt_l_min = ALT_L_NOM * (1.0 - L_TOL)
    alt_ripple_worst = ripple_current(VIN_MAX, alt_l_min, FSW_MIN)
    alt_peak_8a = VOUT_MAX_CURRENT + alt_ripple_worst / 2.0
    alt_rms_6a = rms_inductor_current(VOUT_CONT_CURRENT, alt_ripple_worst)
    alt_rms_8a = rms_inductor_current(VOUT_MAX_CURRENT, alt_ripple_worst)
    peak_8a = VOUT_MAX_CURRENT + ripple_worst / 2.0
    rms_6a = rms_inductor_current(VOUT_CONT_CURRENT, ripple_worst)
    rms_8a = rms_inductor_current(VOUT_MAX_CURRENT, ripple_worst)

    # SY8388A data-sheet equations: input capacitor RMS worst case is IOUT/2.
    cin_rms_bound = VOUT_MAX_CURRENT / 2.0
    # Worst duty is at minimum input.  The data-sheet advertises ~75% maximum duty.
    d_min_vin = duty(VIN_MIN)
    d_max_vin = duty(VIN_MAX)
    p_sw_6_min_vin = mosfet_conduction_loss(VIN_MIN, VOUT_CONT_CURRENT)
    p_sw_6_max_vin = mosfet_conduction_loss(VIN_MAX, VOUT_CONT_CURRENT)
    p_sw_8_min_vin = mosfet_conduction_loss(VIN_MIN, VOUT_MAX_CURRENT)
    p_sw_8_max_vin = mosfet_conduction_loss(VIN_MAX, VOUT_MAX_CURRENT)
    p_allow = (TJ_RECOMMENDED_MAX - TA_MAX) / THETA_JA

    # Output-capacitance lower screen using the SY8388A transient equation and
    # a 0.20 V capacitive undershoot target for a 0->8 A step.
    ton = d_min_vin / FSW_NOM
    toff_min = 150e-9
    dmax = ton / (ton + toff_min)
    dv_cap_target = 0.20
    delta_v_term = VIN_MIN * dmax - VOUT_TARGET
    cout_min = l_max * VOUT_MAX_CURRENT**2 / (2.0 * dv_cap_target * delta_v_term)

    cin_ripple = VOUT_MAX_CURRENT / (4.0 * FSW_MIN * CIN_EFFECTIVE_MIN)
    cout_ripple_cap = ripple_worst / (8.0 * COUT_EFFECTIVE_MIN * FSW_MIN)
    cout_ripple_esr = ripple_worst * COUT_ESR_MAX
    cout_ripple_total = cout_ripple_cap + cout_ripple_esr

    assert ripple_worst / VOUT_MAX_CURRENT <= 0.40
    assert alt_ripple_worst / VOUT_MAX_CURRENT > 0.40
    assert peak_8a < 10.0
    assert alt_peak_8a < 10.0
    assert COUT_EFFECTIVE_MIN >= cout_min

    print("SY8388ARHC first-pass buck screen: PASS topology / component MPNs pending")
    print(f"feedback: VOUT nominal/min/max={vout_nom:.4f}/{vout_min:.4f}/{vout_max:.4f} V")
    print(f"duty: VIN=9.0/12.6 V -> {d_min_vin:.4f}/{d_max_vin:.4f}; max headline duty=0.75")
    print(f"inductor: nominal={L_NOM * 1e6:.2f} uH, +/-20%={l_min * 1e6:.2f}..{l_max * 1e6:.2f} uH")
    print(f"worst ripple at 12.6 V, 510 kHz={ripple_worst:.3f} A")
    print(f"inductor peak at 8 A={peak_8a:.3f} A; RMS at 6/8 A={rms_6a:.3f}/{rms_8a:.3f} A")
    print(
        "2.2 uH comparison at -20% tolerance: "
        f"ripple={alt_ripple_worst:.3f} A ({alt_ripple_worst/VOUT_MAX_CURRENT*100:.2f}%), "
        f"peak={alt_peak_8a:.3f} A, RMS 6/8 A={alt_rms_6a:.3f}/{alt_rms_8a:.3f} A; "
        "exceeds the 40% design screen"
    )
    print(f"input capacitor RMS current bound at 8 A={cin_rms_bound:.3f} A")
    print(f"SY8388 internal MOS conduction loss at 6 A, VIN 9/12.6={p_sw_6_min_vin:.3f}/{p_sw_6_max_vin:.3f} W")
    print(f"SY8388 internal MOS conduction loss at 8 A, VIN 9/12.6={p_sw_8_min_vin:.3f}/{p_sw_8_max_vin:.3f} W")
    print(f"eval-board thermal power screen at TA=40 C, TJ=125 C: {p_allow:.3f} W")
    print(f"CIN effective={CIN_EFFECTIVE_MIN*1e6:.0f} uF -> worst capacitive ripple screen={cin_ripple*1e3:.1f} mV")
    print(f"COUT lower screen for 0->8 A and 0.20 V capacitive step={cout_min * 1e6:.1f} uF effective")
    print(f"COUT selected effective minimum={COUT_EFFECTIVE_MIN*1e6:.0f} uF, ESR<={COUT_ESR_MAX*1e3:.0f} mOhm -> switching ripple<={cout_ripple_total*1e3:.1f} mV")
    print("next: choose exact 2.7 uH >=12 A inductor, >=4 A RMS CIN network, and >=330 uF effective/<=10 mOhm COUT network")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
