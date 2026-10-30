# GLM battery-front-end recalculation — acceptance review v0.2

Status: **ACCEPTED AS CORRECTED CALCULATION DRAFT; NOT ACCEPTED AS ENGINEERING CLOSURE**

Date: 2026-08-30

Reviewed artifacts: repository-root `glm_battery_frontend_draft.md` (22:41 revision)
and `verify_glm_battery_frontend.py` (22:40 revision). Authority remains
[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md`](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md).

## Acceptance result

The recalculation materially fixes the seven findings in
[`GLM_BATTERY_FRONTEND_VALIDATION_v0.1.md`](GLM_BATTERY_FRONTEND_VALIDATION_v0.1.md):

- JMSL maximum `RDS(on)` values are now used;
- LMX `ISENSE` is correctly labelled typical;
- the fuse curve/data distinction is corrected;
- unfinished TVS/OVLO/PWR/TIMER work is correctly classified as Engineering Design
  Decision or Prototype Validation Risk rather than automatically as CDB;
- raw `VDS x I` values are correctly limited to negative screening;
- `RSENSE=6.2 mOhm +/-1%` is correctly interpreted against the frozen load envelope;
- MX74610's 180 uA maximum reverse leakage is now disclosed.

The Python output and arithmetic reproduce. `py_compile` and whitespace checks pass.
Running with `--cout-uf 220 --cout-tol 0.20` correctly reports:

```text
effective 220 uF: Eq.10 startup = 1.738 ms
1.5 x startup / tFAULT,min = 0.860
nominal COUT limit at +/-20% = 213.2 uF
```

Thus an **effective** 220 uF value passes the simplified no-load screen, while a
nominal 220 uF +/-20% part does not guarantee it because its high corner is 264 uF.

## Items that remain unaccepted

1. `PLIM=14 W`, `RPWR=13.356 kOhm`, and `CTIMER=100 nF` remain calculation points,
   not selected values. The 10.44--17.69 W range is explicitly a first-order scaling
   of `PWRILM`; it is not a guaranteed full transfer corner.
2. The JMSL Figure 9/11 curve has not been numerically compared with the worst
   `VDS(t), I(t), tFAULT` point. Reporting required `ZthetaJC > 1` demonstrates that
   transient thermal impedance alone is non-controlling, but does not replace the
   linear-mode SOA curve.
3. The actual YSIDO input capacitance and startup load remain unknown. The 255.9 uF
   result is a no-load equation bound, not a module guarantee.
4. The automatic-retry 1.80% / 0.251 W result is a non-guaranteed timing screen. The
   final threshold lacks a guaranteed corner and no decision has selected automatic
   retry versus hardware latch-off.
5. Direct MX74610 reverse exposure still fails the inactive-port target by 80 uA.
   The source-isolation topology has not yet proved that the condition is impossible.
6. No exact TVS or clamp voltage with margin below the 30 V MOSFET rating has been
   selected. A 30 V clamp screen is not acceptable as a 30 V MOSFET design point.
7. OVLO thresholds and LMX branch scope remain undecided.

There is also one internal text defect: section C.3 correctly states that the
10.590 A limit corner is not a Freeze conflict, but section C.4 retains the old claim
that the no-intersection result requires an Operator priority decision. C.4 must be
rewritten to match C.3 before the draft can be promoted unchanged.

## Disposition

- Recalculated arithmetic: **PASS**.
- Correction of v0.1 review findings: **PASS with the C.4 editorial defect noted**.
- Final PWR/TIMER/SOA decision: **OPEN**.
- Reverse-current compliance: **OPEN; direct-path case fails**.
- Battery-front-end topology selection: **NOT YET ACCEPTED**.

No schematic, ERC, netlist, PCB layout, purchase, or physical test is authorized by
this acceptance review.
