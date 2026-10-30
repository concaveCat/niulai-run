# Decision 019: MPU6500 posture fall as a game failure boundary

Status: Testing

Date: 2026-09-17

Risk: HIGH RISK

## Context

The game needs a deterministic failure boundary when the physical robot tips.
The existing MPU6500 path exposed read-only acceleration telemetry but did not
connect posture to the game, safety gate, or Android result state.

## Decision

- Use filtered relative MPU6500 Roll/Pitch after a 25-sample static baseline.
- Assert `FALL_DETECTED` when either absolute angle is strictly above 60 degrees
  for three consecutive valid samples.
- Clear only when both angles are below 50 degrees for 25 consecutive valid
  samples. Clearing requires a fresh safety-gate acceptance window and never
  replays the previous remote input mask.
- On assertion, clear remote input, revoke/close the safety gate, and use the
  existing actuator safe-stop path so `/OE` remains HIGH. Do not change the
  BLE control packet or GAME_MODE semantics; add STATUS bit 5 only.
- Android physical gameplay converts a new fall status into a failed result
  with speed and coordination scores equal to zero. AI/local feedback must
  describe the fall and must not praise a successful action.
- TFT renders a full-red frame that linearly fades to black for approximately
  one second, then returns to the normal gameplay renderer.

## Verification

Pure detector, Roll/Pitch math, BLE STATUS, Android model, and display renderer
tests are covered by `sh firmware-esp32/tests/host/run_host_tests.sh`; the
current result is PASS. A fresh ESP-IDF v5.5.5 ESP32-S3 build is in
`firmware-esp32/build-imu-fall-current`; physical flash and tilt-fixture
acceptance remain open. See [`EXP-043`](../../experiments/EXP-043-mpu6500-fall-failure-path.md).

## Consequences and rollback

The safety gate and input protocol gain an explicit posture fault state, and
Android must accept STATUS bit 5. A false positive stops the actuator and
fails the current game, so the threshold and hysteresis require staged hardware
validation. Rollback is to remove the fall detector integration while retaining
the existing MPU6500 telemetry path; do not bypass `/OE` or the safety gate.
