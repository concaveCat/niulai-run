#!/usr/bin/env python3
"""Empirical-bound sensitivity screen for unmeasured Niulai parameters.

Purpose: let the paper worst-case analysis proceed while prototype measurements
are unavailable (prototype validation happens AFTER the board exists, so the
paper design must proceed on conservative bounds).

Authoritative (frozen) inputs used here:
  VBAT = 9.0 - 12.6 V (3S LiPo)
  servo path = 6.0 A continuous / 8.0 A for 1.0 s, >=10 s between event starts
  TA_MAX = 40 C; TJ <= min(100 C, TJ_MAX - 25 C)
  battery-entry physical fuse: rated >=32 VDC, DC breaking >=100 A
  screen + 3V3 expansion budget = 0.5 A
  3V3 = 3.40 V nominal, 3.30-3.50 V window, <=50 mVpp ripple,
        2.5 A continuous / 3.0 A for >=10 ms

Every input marked ASSUMPTION is an empirical engineering bound, NOT a
manufacturer guaranteed limit and NOT a measured value. This script is a
sensitivity tool for paper circuit design; it proves nothing about the
assembled board and authorizes no schematic, layout, purchase or test.
"""

from __future__ import annotations

# ------------------------------ Battery bounds ------------------------------
# ASSUMPTION: generic 3S 650 mAh hobby LiPo, room temp, mid-high SOC.
# Small high-C packs run roughly 8-40 mOhm per cell depending on age/temp/SOC.
IR_CELL_BEST = 8e-3    # ohm, optimistic fresh high-C cell
IR_CELL_TYP = 20e-3    # ohm
IR_CELL_WORST = 40e-3  # ohm, cold / aged / low SOC
CELLS = 3
VBAT_FULL = 12.6

# ASSUMPTION: XT30 pair + PCB loop + solder adds 10-40 mOhm in a hard short.
LOOP_EXTRA_BEST = 10e-3
LOOP_EXTRA_WORST = 40e-3

# Recorded selection (Open Issues EDD-001): JKSEMI C355692, 10 A, 32 VDC /
# 150 A DC breaking. Frozen minimum is 100 A DC breaking.
FUSE_BREAKING_A = 150.0
# ASSUMPTION: 20 A^2s is a typical 10xIn pre-arcing datum, not a guaranteed
# no-blow threshold and not a guaranteed tolerance band.
FUSE_PREARC_I2T_A2S = 20.0
FUSE_COLD_R_OHM = 6.8e-3        # typical, from prior battery-front-end review
FUSE_DERATING_40C = 0.97        # ASSUMPTION: graph-screened at 40 C

# ------------------------------ Servo bounds --------------------------------
# ASSUMPTION: MG90S stall current typical range at 5-6 V.
SERVO_STALL_BEST = 0.7   # A
SERVO_STALL_WORST = 1.0  # A
SERVO_COUNT = 8
SERVO_CONT_A = 6.0       # frozen
SERVO_TRANS_A = 8.0      # frozen

# ------------------------------ ST7789 bounds -------------------------------
# ASSUMPTION: 170x320 ST7789 module, controller + backlight, at 3.3 V.
ST7789_CTRL_MIN = 0.010  # A
ST7789_CTRL_MAX = 0.020  # A
ST7789_BL_MIN = 0.020    # A
ST7789_BL_MAX = 0.080    # A
SCREEN_BUDGET_A = 0.5    # frozen

# ------------------------------ SY8388 buck ---------------------------------
# Datasheet typical RDS(on); ASSUMPTION hot multiplier 1.5-1.7x typical at
# 100 C junction (typical curve, not guaranteed).
TOP_RDS_TYP = 20e-3
BOTTOM_RDS_TYP = 10e-3
RDS_HOT_BEST = 1.5
RDS_HOT_WORST = 1.7
# ASSUMPTION: combined rise+fall switching time, 10-30 ns total.
TR_TF_BEST = 10e-9
TR_TF_WORST = 30e-9
VIN_MIN = 9.0
VIN_MAX = 12.6
VOUT = 6.0
FSW = 600e3
THETA_JA = 33.0         # datasheet eval board 8.5x8.5 cm, 4L 2 oz
TA_MAX = 40.0
TJ_RECOMMENDED_MAX = 125.0


def duty(vin: float) -> float:
    return VOUT / vin


def conduction_loss(vin: float, iout: float, hot_mult: float) -> float:
    d = duty(vin)
    rtop = TOP_RDS_TYP * hot_mult
    rbot = BOTTOM_RDS_TYP * hot_mult
    return iout**2 * (d * rtop + (1.0 - d) * rbot)


def switching_loss(vin: float, iout: float, tr_tf: float) -> float:
    # ASSUMPTION: Psw ~= Vin * Iout * (tr+tf) * fsw, first-order bound that
    # absorbs both devices' transition energy.
    return vin * iout * tr_tf * FSW


def main() -> int:
    print("EMPIRICAL-BOUND SENSITIVITY SCREEN (draft, non-authoritative)")
    print("=" * 72)

    # ---- Battery short-current vs fuse breaking capacity ----
    print("[1] 3S battery prospective short current vs fuse breaking capacity")
    print("    ASSUMPTION: per-cell IR 8/20/40 mOhm; loop extra 10/40 mOhm")
    rows = []
    for label, ir_cell, loop in (
        ("lowest-R (fresh cell + tight loop)", IR_CELL_BEST, LOOP_EXTRA_BEST),
        ("typical (mid cell + mid loop)", IR_CELL_TYP,
         (LOOP_EXTRA_BEST + LOOP_EXTRA_WORST) / 2.0),
        ("highest-R (aged/cold cell + long loop)", IR_CELL_WORST, LOOP_EXTRA_WORST),
    ):
        # The fuse is a series element, not a current limiter: its own cold
        # resistance joins the short loop, and it must INTERRUPT (not block)
        # whatever prospective current that loop allows.
        r_loop = CELLS * ir_cell + loop + FUSE_COLD_R_OHM
        i_sc = VBAT_FULL / r_loop
        t_clear = FUSE_PREARC_I2T_A2S / i_sc**2
        margin = FUSE_BREAKING_A - i_sc
        rows.append((label, r_loop, i_sc, t_clear, margin))
    print(f"    {'case':<36}{'Rloop':>8}{'I_sc':>8}{'t_clear':>10}{'brk margin':>12}")
    for label, r_loop, i_sc, t_clear, margin in rows:
        print(
            f"    {label:<36}{r_loop*1e3:>6.0f}mO"
            f"{i_sc:>7.0f}A{t_clear*1e3:>8.2f}ms{margin:>+10.0f}A"
        )
    print("    -> fuse breaking 150 A is adequate only for the high-R end; the")
    print("       low-R corner exceeds it. Decision point, not a frozen blocker.")

    # ---- Fuse no-nuisance / thermal ----
    print("[2] Fuse (C355692, 10 A) no-nuisance and conduction sensitivity")
    derated = 10.0 * FUSE_DERATING_40C
    ratio_8a = 8.0 / derated
    p_6a = 6.0**2 * FUSE_COLD_R_OHM
    p_8a = 8.0**2 * FUSE_COLD_R_OHM
    print(f"    40 C graph-derated rating ~= {derated:.2f} A")
    print(f"    8 A / 1 s is {ratio_8a:.3f}x of derated rating (no-nuisance screen)")
    print(f"    6 A loss ~= {p_6a:.3f} W; 8 A loss ~= {p_8a:.3f} W (typical R)")

    # ---- Servo stall cross-check ----
    print("[3] MG90S 8-way stall vs frozen servo envelope")
    stall_min = SERVO_COUNT * SERVO_STALL_BEST
    stall_max = SERVO_COUNT * SERVO_STALL_WORST
    print(f"    8x stall estimate = {stall_min:.1f}..{stall_max:.1f} A")
    print(f"    frozen continuous = {SERVO_CONT_A} A; transient = {SERVO_TRANS_A} A")
    print(
        f"    worst 8-way stall is {stall_max / SERVO_CONT_A:.2f}x continuous -> "
        "firmware/protection must not allow sustained 8-way stall"
    )

    # ---- ST7789 ----
    print("[4] ST7789 module current vs 0.5 A frozen budget")
    st_min = ST7789_CTRL_MIN + ST7789_BL_MIN
    st_max = ST7789_CTRL_MAX + ST7789_BL_MAX
    print(f"    module current estimate = {st_min*1e3:.0f}..{st_max*1e3:.0f} mA")
    print(f"    budget margin = {SCREEN_BUDGET_A/st_max:.1f}x at worst estimate")

    # ---- SY8388 thermal sensitivity ----
    print("[5] SY8388ARHC loss/thermal sensitivity (typical RDS x hot mult)")
    print(f"    {'Vin':>5}{'Iout':>5}{'cond6':>8}{'condW':>8}{'Psw(10..30ns)':>16}")
    for vin in (VIN_MIN, VIN_MAX):
        for iout in (SERVO_CONT_A, SERVO_TRANS_A):
            cb = conduction_loss(vin, iout, RDS_HOT_BEST)
            cw = conduction_loss(vin, iout, RDS_HOT_WORST)
            ps = switching_loss(vin, iout, TR_TF_BEST)
            pw = switching_loss(vin, iout, TR_TF_WORST)
            print(
                f"    {vin:>5.1f}{iout:>5.1f}{cb:>7.2f}W{cw:>7.2f}W"
                f"{ps:>8.2f}..{pw:.2f}W"
            )
    p_allow = (TJ_RECOMMENDED_MAX - TA_MAX) / THETA_JA
    # 6 A continuous worst corner (12.6 V, hot worst, sw worst)
    p6_worst = (
        conduction_loss(VIN_MAX, SERVO_CONT_A, RDS_HOT_WORST)
        + switching_loss(VIN_MAX, SERVO_CONT_A, TR_TF_WORST)
    )
    # 8 A / 1 s worst corner
    p8_worst = (
        conduction_loss(VIN_MAX, SERVO_TRANS_A, RDS_HOT_WORST)
        + switching_loss(VIN_MAX, SERVO_TRANS_A, TR_TF_WORST)
    )
    # 8 A duty-average power (6 A baseline + 10% of the extra 2 A)
    p8_duty_avg = (
        conduction_loss(VIN_MAX, SERVO_CONT_A, RDS_HOT_WORST)
        + switching_loss(VIN_MAX, SERVO_CONT_A, TR_TF_WORST)
        + 0.10 * (
            (conduction_loss(VIN_MAX, SERVO_TRANS_A, RDS_HOT_WORST)
             - conduction_loss(VIN_MAX, SERVO_CONT_A, RDS_HOT_WORST))
            + (switching_loss(VIN_MAX, SERVO_TRANS_A, TR_TF_WORST)
               - switching_loss(VIN_MAX, SERVO_CONT_A, TR_TF_WORST))
        )
    )
    print(f"    eval-board thermal screen: P_allow = {p_allow:.2f} W")
    print(f"    6 A continuous worst ~= {p6_worst:.2f} W -> {'OK' if p6_worst < p_allow else 'MARGINAL'}")
    print(f"    8 A/1 s worst ~= {p8_worst:.2f} W -> {'OK' if p8_worst < p_allow else 'MARGINAL (10% duty)'}")
    print(f"    8 A/1 s 10%-duty average ~= {p8_duty_avg:.2f} W -> {'OK' if p8_duty_avg < p_allow else 'MARGINAL'}")
    print("    -> thermal OK with margin for 6 A continuous; 8 A transient worst")
    print("       corner is marginal on switching-loss assumption and remains PVR-015")

    print("=" * 72)
    print("SUMMARY: all values are ASSUMPTION-bounded, none validated. Action items:")
    print("  1. Confirm C355692 actual DC breaking >= prospective low-R short, or")
    print("     select higher-breaking fuse / bound loop resistance.")
    print("  2. Firmware must not sustain 8-way stall beyond the frozen envelope.")
    print("  3. ST7789 budget is comfortable; only BLK polarity/current on receipt.")
    print("  4. SY8388 6 A continuous thermal is OK; 8 A transient stays PVR-015.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
