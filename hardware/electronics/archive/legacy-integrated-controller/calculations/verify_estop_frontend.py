#!/usr/bin/env python3
"""Paper timing screen for the external normally-closed ESTOP input.

This verifies only the passive 1 kOhm / 10 kOhm / 100 nF front end.
It deliberately uses conservative CMOS input guarantees (VIL=0.3*VDD,
VIH=0.7*VDD) rather than a particular Schmitt-gate typical threshold.
"""

from __future__ import annotations

import math

R_SER_NOM = 1_000.0
R_PULLDOWN_NOM = 10_000.0
R_TOL = 0.01
C_NOM = 100e-9
C_TOL = 0.10

VIL_RATIO = 0.30
VIH_RATIO = 0.70
SHUTDOWN_LIMIT = 10e-3


def discharge_time(r: float, c: float, ratio: float) -> float:
    return -r * c * math.log(ratio)


def charge_time(r_series: float, r_pd: float, c: float, target_ratio: float) -> float:
    v_final_ratio = r_pd / (r_series + r_pd)
    r_th = r_series * r_pd / (r_series + r_pd)
    if target_ratio >= v_final_ratio:
        raise ValueError("target cannot be reached")
    return -r_th * c * math.log(1.0 - target_ratio / v_final_ratio)


def main() -> int:
    rpd_min = R_PULLDOWN_NOM * (1.0 - R_TOL)
    rpd_max = R_PULLDOWN_NOM * (1.0 + R_TOL)
    rser_max = R_SER_NOM * (1.0 + R_TOL)
    c_min = C_NOM * (1.0 - C_TOL)
    c_max = C_NOM * (1.0 + C_TOL)

    # Loop opens: capacitor discharges only through the 10 kOhm pulldown.
    t_open_min = discharge_time(rpd_min, c_min, VIL_RATIO)
    t_open_max = discharge_time(rpd_max, c_max, VIL_RATIO)

    # Loop closes: AON_3V3 drives through 1 kOhm while 10 kOhm remains to GND.
    t_close_max = charge_time(rser_max, rpd_max, c_max, VIH_RATIO)

    assert t_open_min >= 1e-3
    assert t_open_max < SHUTDOWN_LIMIT
    assert t_close_max < 1e-3

    print("ESTOP passive front-end timing: PASS")
    print(f"- loop-open guaranteed-low screen: {t_open_min*1e3:.3f}..{t_open_max*1e3:.3f} ms")
    print(f"- loop-close guaranteed-high screen: <= {t_close_max*1e3:.3f} ms")
    print("- remaining <10 ms budget is available for Schmitt/latch/gate propagation")
    print("- closure does not re-arm because the downstream hardware fault latch retains state")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
