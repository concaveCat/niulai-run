# EXP-034 ARD-1 PastureStep-Reserve adaptive-reserve gait

## Status

Testing — offline engineering plus first controlled physical slice; this is
not physical gait acceptance.

## Objective

Engineer and test a gait with large visible swing, independently bounded body
drive, and per-leg travel reserve, while preserving the current 120 ms landing
time and a bent-knee touchdown.

## Hypothesis

Separating swing distance from body drive will produce a larger visible step
without forcing support feet to retreat by the active-leg swing distance.
Applying the same body-frame `-D` to each support foot models body translation;
per-leg reserve and independent limit handling should prevent cumulative stall.

## Hardware version

Physical target is ESP32-S3 + PCA9685 four 2-DOF legs. The initial engineering
stage was mock/host mathematics only; a controlled COM4 flash and diagonal
physical slice was later run on 2026-09-14 under the safety gate recorded in
the live log.

## Software commit

Source baseline commit `a2ce962` on `fw/kw10-perfect-touchdown`; the current
offline repair slice is an uncommitted working-tree change on top of that
baseline. The earlier physical run used the image built from the baseline.

## Parameters

Initial hypotheses, not frozen values:

- safe usable span `S_i` is per leg and derived from actual safe x endpoints;
- visible swing target `0.45..0.90 S_i` as hold increases;
- body drive target approximately `0.16..0.38 S`;
- reserve margin `0.85` is a tunable hypothesis;
- lift remains the current `40 mm` until clearance is measured;
- `lift_ms=360 ms`, `search_ms=120 ms`; initial swing timing is `300 ms`;
- no KW10/IMU/encoder feedback in the first offline model.
- physical validation profile used `30,9,40,360,300,120,50`; this remains an
  experimental candidate, not a frozen mechanical setting.

## Test method

1. Add an explicit `body_drive_mm` state/output separate from active swing.
2. Verify each support leg receives the same `-D` request before its own
   independent workspace/pulse clipping.
3. Test hold targets, saturation, rapid RF/LF/RR/LR rotation, and diagonal
   swing. Assert no hidden fixed `swing_ms` stop and no accumulated support
   reserve underflow.
4. Compare ARD-1 against archived KFC-1 for foot x, pulse continuity, support
   masks, and landing geometry.
5. Only after offline acceptance, request low-amplitude physical tests. This
   gate was satisfied for the controlled 2026-09-14 slice; physical acceptance
   remains open.

## Video / logs

No physical video was captured by the agent. The controlled COM4 serial record
is [`EXP-034 live COM4 log`](logs/EXP-034-2026-09-14-ard1-com4-live.txt).

## Result

Architecture archived as `KFC-1 / PastureStep-Conserve`; ARD-1 is the active
successor engineering target. The third offline slice replaces the repeatedly
shrunk `85% of remaining capacity` budget with a fixed per-leg safe rear
endpoint for each landed support cycle. Existing supports retain that endpoint
across rebases; only a newly landed leg establishes a fresh one. The common
body-drive request follows a fifth-order smoothstep in the middle `20%..85%`
of swing. Each support independently records the amount it actually applies,
so the common request is not claimed as measured body translation. Normal
bounded-target completion has separate telemetry from geometry/pulse soft
limits. Landing rebase consumes the last successfully written pose directly,
including the partial diagonal case where another leg remains active.

Strict host tests cover fixed-boundary persistence, asymmetric applied amounts,
middle-window drive, bounded hold, release continuity, partial landing rebase,
the cached live swing boundary, diagonal motion under the current independent
hip limits, and a 64-step rotation under those same limits. Live runtime
progress now advances one successful control frame at a time, and the
sequence host test holds through the full lift plus swing interval. ESP-IDF
build succeeds. A first controlled physical slice was then run on COM4 under
the Operator-confirmed safety gate: RF single-leg, LF+RR low-amplitude, LF+RR
default-profile, and one default-profile LF+RR→RF+LR diagonal cycle all
reached the firmware completion path. The serial result is not physical gait
acceptance: no visual/video/current/thermal observation was captured by the
agent. An attempted change that
made the seventh UART field speed-only and exposed the full `30 mm` target
failed the existing LF mirrored hip-limit regression and was reverted. The
follow-up `50%` visible-swing candidate (`30,9,40,360,300,120,50`) passes all
current offline tests and is now the default; it remains a software candidate,
not a physical claim.

## 2026-09-14 offline repair slice

This slice addresses two observations from the previous physical/software
baseline without changing the I²C configuration or retry policy:

- Live body-drive is now `100%` of the active foot-space swing target, rather
  than the previous fixed `35%`. With the default `step=30 mm` and
  `hip_scale=50%`, the nominal common support retreat is therefore about
  `15 mm`; each support still clips independently against the already-defined
  per-leg workspace and one-sided hip pulse limit.
- Support safe-rear endpoints are carried across separate live press/release
  windows. Starting another leg no longer reconstructs a fresh reserve from
  the current, already-retreated foot, which was the source of progressively
  shrinking effective limits during continuous walking. A newly landed leg is
  still rebased from the last successfully written pose.
- Touchdown keeps only `25%` of the active horizontal swing displacement as a
  tunable software contact candidate. The fixed `120 ms` SEARCH timing is
  unchanged. This reduces the forward touchdown offset and moves the
  point-foot model closer to a bent, load-bearing shank; it cannot establish
  that the physical sole is fully flat without a sole/contact-patch model and
  bench observation.
- The Windows host forwards a release even when a short press received no
  correlated acceptance line in its brief UART drain. This makes a delayed
  acceptance unable to leave the firmware's knee in an indefinite held state.
  A literal firmware rejection is classified as rejection, not queued work.

The engineering acceptance evidence is the strict quad C suite, sanitizer
runs, the host keyboard tests, the full offline check entry point, and an
ESP-IDF build. The resulting image was subsequently flashed once to COM4 and
passed a boot/list/stats/stop check, but no movement command was sent in that
follow-up. No claim is made yet about physical forward travel, sole contact,
falls, current, or thermal behavior.

Reproduction commands and result for this slice:

- `python3 -m unittest tools.tests.test_quad_ascii_walk -v`: 18 tests passed;
- `cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/servo_zero_leg_math.c main/servo_zero_quad_demo.c tests/test_servo_zero_quad_demo.c -lm -o /tmp/niulai-servo-zero-quad-demo-test && /tmp/niulai-servo-zero-quad-demo-test`: pass;
- the same C command with `-fsanitize=address,undefined` plus
  `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`: pass;
- `sh tools/run_offline_checks.sh`: pass, including 41 tool tests, 14 firmware
  Python tests, 8 pure-C host suites, documentation audits and diff check;
- `ninja -C firmware-esp32/tools/servo-zero-test/build-servo-zero`: pass,
  image `0xd9ba0`, smallest app partition 15% free.

The first focused run after changing touchdown retention exposed a 165 us
first SEARCH-frame hip jump; this was fixed by the explicit linear foot-space
transition and the focused C/sanitizer tests were then rerun successfully.
The flash/boot evidence is [`EXP-034 ARD-1 repair flash log`](logs/EXP-034-2026-09-14-ard1-repair-flash-boot.txt).
No physical motion log or video exists for this follow-up; the prior
controlled COM4 motion record remains [`EXP-034 live COM4 log`](logs/EXP-034-2026-09-14-ard1-com4-live.txt).

Validation commands and results for this slice:

- strict C with ASan/UBSan: pass, `servo_zero_quad_demo tests passed`;
- `ninja -C firmware-esp32/tools/servo-zero-test/build-servo-zero`: pass,
  image `0xd9ad0`, smallest app partition has 15% free;
- `sh tools/run_offline_checks.sh`: pass, including 39 tool tests, all ESP32
  host suites, documentation indexes and `git diff --check`;
- Android tests were not requested by the offline runner and were skipped;
- controlled COM4 run: all commands completed and the final stop returned
  `/OE HIGH`; the two-step diagonal sequence reported
  `logical=338 attempts=345 success=338 nack=7 timeout=0 other=0 retries=7
  recovered_after_retry=5 terminal_failures=0`. Transient I2C timeout/NACK
  diagnostics remain an open bus-robustness finding; see the live log.

## Next step

Record the Operator's visual/current/thermal observations and investigate why
the transient software-timeout diagnostic did not increment the final timeout
counter. Then add the one-entry input buffer and explicit
requested-versus-estimated body-motion status report, and compare ARD-1
against KFC-1 before increasing amplitude or repeating many cycles.
