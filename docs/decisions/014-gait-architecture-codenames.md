# Decision 014: 牛来快跑 gait architecture archive and ARD-1 successor

Status:
- Testing

Problem:
牛来快跑 needs a traceable boundary between the current experimentally
validated live-hold gait and the proposed large-swing/adaptive-drive gait.
Mixing their support-motion semantics would make test results ambiguous.

Archived current architecture:
- Codename: `KFC-1 / PastureStep-Conserve`（牧步-守恒）
- Knee-first lift, foot-space IK, live hold-to-swing, timed 120 ms SEARCH.
- Active and support foot displacement use a conservation allocator; support
  legs retain independent workspace and pulse-limit clipping.
- This is an archived software baseline, not an accepted physical gait.

Successor under engineering:
- Codename: `ARD-1 / PastureStep-Reserve`（牧步-储程）
- Separate `swing_distance_mm` from `body_drive_mm`.
- During support, each support foot receives the same body-frame `-D` request,
  then independently clips it at its own fixed safe rear endpoint. The common
  request is not measured body translation.
- Hold time selects a bounded swing/drive target, then the target is held;
  SEARCH stops horizontal drive and retains the 120 ms landing requirement.

Current offline repair candidate (2026-09-14): live body-drive reaches `100%`
of the active foot-space target under the default profile, while touchdown
retains only `25%` of the horizontal swing offset. Safe-rear endpoints persist
across separate press/release windows and remain independently clipped per
leg. These values are software candidates pending renewed physical gait,
sole-contact, current and fall observations.

Decision boundary:
`KFC-1` remains the documented experiment baseline and rollback reference;
the current source does not yet provide a runtime mode switch. `ARD-1` must first pass host geometry,
limit and trajectory tests, then low-amplitude single-leg and diagonal bench
tests. No physical claim is transferred from KFC-1 to ARD-1.

Open engineering constraints:
- Per-leg safe x intervals must be measured/derived from each leg's IK and
  q→us limits; a universal 10% margin is only a starting hypothesis.
- The accepted rule that one support leg reaching its limit must not stop all
  other support legs remains in force; ARD-1 must not silently replace it with
  a global minimum-stop rule.
- Current 40 mm lift remains until physical clearance supports reducing it.
- KW10/IMU/encoder feedback is not assumed by this isolated v0 experiment.

Experiment:
[`EXP-034`](../../experiments/EXP-034-ard1-adaptive-reserve-gait.md) defines
the first offline ARD-1 engineering and test sequence.

Result:
KFC-1 has been archived in the 2026-09-14 EXP-028 records. ARD-1 implementation
and validation are in progress. The repair candidate is covered by offline
tests only; no physical acceptance is transferred by this record.

Consequences:
The isolated quad demo API and tests now have an explicit body-drive model,
bounded hold target, fixed per-leg rear safety endpoints, per-leg applied-drive
telemetry, and written-pose atomic rebase;
the existing KFC-1 path remains available as a comparison/rollback fixture.
README and experiment records must identify which codename a result belongs to.
