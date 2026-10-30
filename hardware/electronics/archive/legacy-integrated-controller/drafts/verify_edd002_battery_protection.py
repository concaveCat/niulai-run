#!/usr/bin/env python3
"""EDD-002 battery-entry active protection screening (LTC4368-2 + HD303N080SG).

Real device data (from local HD303N080SG datasheet REV1.0, Zhuhai
Hongjiacheng, accessed 2026-08-30):
  VDS = 30 V; ID = 28 A; IDM = 100 A
  RDS(on) max = 8 mOhm @ VGS=10 V (11 mOhm @ 4.5 V)
  RthJC = 6 C/W; package PDFN3030 (3x3 DFN, exposed pad)
  TJ = -55..+150 C

Controller constants marked ASSUMPTION are engineering bounds pending
verification against LTC4368 Rev C datasheet (current-limit threshold,
gate pull-up current, TMR equation). This script is a paper sensitivity
tool, not a proof and not prototype validation.
"""

from __future__ import annotations

# ---- HD303N080SG real data ----
RDS_MAX_10V = 8e-3        # ohm per FET, VGS=10 V
RDS_HOT_MULT = 1.5        # ASSUMPTION: Fig.2 typical ratio at ~100 C
RTH_JC = 6.0              # C/W per FET
RTH_JA_BEST = 40.0        # ASSUMPTION: PDFN3030 on 4L 2 oz, good spreading
RTH_JA_WORST = 60.0       # ASSUMPTION: poorer spreading
IDM = 100.0               # A pulsed

# ---- LTC4368-2 constants (ASSUMPTION pending Rev C verification) ----
VCL_TYP = 50e-3           # V, forward circuit-breaker sense threshold
VCL_MIN = 40e-3           # V, sensitivity band (NOT guaranteed spec)
VCL_MAX = 60e-3           # V, sensitivity band
REV_THRESH_MIN = 1e-3     # V, -2 variant reverse threshold min magnitude
REV_THRESH_MAX = 5e-3     # V, reverse threshold max magnitude

# ---- Frozen envelopes ----
VBAT_MAX = 12.6
BUCK_CONT_A = 6.0         # SERVO_BUCK_IN continuous
BUCK_TRANS_A = 7.5        # SERVO_BUCK_IN 1 s / >=10 s
BUCK_INRUSH_A = 10.0      # startup/inrush 10 ms
LOGIC_BRANCH_A = 1.5      # ASSUMPTION: LOGIC_BAT from VBAT, bounding
TA_MAX = 40.0
TJ_STEADY_MAX = 100.0     # DIB-002: min(100, TJ_MAX-25)


def main() -> int:
    print("EDD-002 battery protection screen (LTC4368-2 + HD303N080SG)")
    print("=" * 72)

    # ---- 1. Conduction loss + thermal (real RDS) ----
    print("[1] Back-to-back conduction loss and junction temperature")
    total_env = {
        "7.5 A battery continuous (6 A servo + logic bound)": BUCK_CONT_A + LOGIC_BRANCH_A,
        "9.0 A battery transient (7.5 A servo + logic bound)": BUCK_TRANS_A + LOGIC_BRANCH_A,
    }
    for label, i_a in total_env.items():
        r_total_hot = 2.0 * RDS_MAX_10V * RDS_HOT_MULT  # two FETs in series
        p_total = i_a**2 * r_total_hot
        p_per_fet = p_total / 2.0
        tj_best = TA_MAX + p_per_fet * RTH_JA_BEST
        tj_worst = TA_MAX + p_per_fet * RTH_JA_WORST
        ok = tj_worst <= TJ_STEADY_MAX
        print(
            f"    {label}: P_total={p_total:.3f} W, per-FET={p_per_fet:.3f} W, "
            f"TJ={tj_best:.0f}..{tj_worst:.0f} C -> {'OK' if ok else 'MARGINAL'}"
        )

    # ---- 2. RSENSE sizing ----
    print("[2] RSENSE options vs total battery current envelope")
    print(
        f"    envelope: continuous={BUCK_CONT_A + LOGIC_BRANCH_A:.1f} A, "
        f"transient={BUCK_TRANS_A + LOGIC_BRANCH_A:.1f} A, "
        f"inrush={BUCK_INRUSH_A + LOGIC_BRANCH_A:.1f} A"
    )
    print("    RSENSE  I_trip min/typ/max   margin vs 9.0 A   P@7.5A   P@9A")
    for rsense in (4e-3, 5e-3, 6.2e-3):
        i_min = VCL_MIN / rsense
        i_typ = VCL_TYP / rsense
        i_max = VCL_MAX / rsense
        envelope = BUCK_TRANS_A + LOGIC_BRANCH_A
        margin = i_min - envelope
        p6 = (BUCK_CONT_A + LOGIC_BRANCH_A) ** 2 * rsense
        p8 = (BUCK_TRANS_A + LOGIC_BRANCH_A) ** 2 * rsense
        flag = "OK" if margin >= 0.5 else ("THIN" if margin >= 0 else "REJECT")
        print(
            f"    {rsense*1e3:.1f}mO  {i_min:5.1f}/{i_typ:5.1f}/{i_max:5.1f} A"
            f"      {margin:+5.2f} A [{flag}]"
            f"   {p6:.3f}W {p8:.3f}W"
        )

    # ---- 3. SOA stress point (linear-mode current limit) ----
    print("[3] SOA stress point for downstream hard short (check Fig.8)")
    for rsense in (4e-3, 5e-3):
        i_lim = VCL_TYP / rsense
        p_lim = VBAT_MAX * i_lim
        print(
            f"    RSENSE={rsense*1e3:.1f}mO: VDS~={VBAT_MAX} V, "
            f"ID(lim)~={i_lim:.1f} A, P~={p_lim:.0f} W for the fault-timer period"
        )
    print("    -> must be checked against HD303 Fig.8 SOA (VDS=12.6 V column);")
    print("       the fault timer (TMR) must keep this pulse short enough.")

    # ---- 4. Reverse-current threshold ----
    print("[4] Reverse-current trip (backfeed prevention, -2 variant)")
    for rsense in (4e-3, 5e-3):
        i_rev_min = REV_THRESH_MIN / rsense
        i_rev_max = REV_THRESH_MAX / rsense
        print(
            f"    RSENSE={rsense*1e3:.1f}mO: reverse trip "
            f"{i_rev_min:.2f}..{i_rev_max:.2f} A"
        )

    print("=" * 72)
    print("SUMMARY:")
    print("  - Conduction/thermal: OK in this screen at 7.5 A battery continuous (real RDS 8 mOhm max).")
    print("  - RSENSE=4 mOhm gives I_trip 10..15 A (40..60 mV band): min trip")
    print("    is 1.0 A above the 9 A envelope (thin under the pessimistic 40 mV")
    print("    min; confirm LTC4368 VCL guaranteed spec). 5/6.2 mOhm are REJECTED")
    print("    (minimum trip below the 9 A envelope).")
    print("  - Startup inrush (11.5 A) exceeds the 4 mOhm trip floor, so the")
    print("    current limit doubles as a soft-start; TMR must outlast the inrush")
    print("    (>=10 ms) or CGATE must slow the buck input ramp below the limit.")
    print("  - Reverse threshold (1..5 mV) trips at 0.2..1.25 A with 4..5 mOhm.")
    print("  - SOA (12.6 V linear-mode limit, ~158 W for TMR) and 30 V TVS clamp")
    print("    margin remain to be closed with HD303 Fig.8 and LTC4368 TMR/CGATE")
    print("    datasheet values.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
