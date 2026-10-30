# Decision 020: Android valid-step scoring candidate

Status: Testing

Date: 2026-09-17

Risk: MEDIUM

## Context

The first Android gameplay prototype scored control-mask changes as progress.
A press and release could therefore look like two units of progress even when
the firmware rejected the command or the leg never left the ground. The old
coordination score also treated a very small sample as rhythmically perfect.

## Decision

For the Android-only scoring candidate, the unit of progress is a completed
valid step rather than a control-mask change.

- Test Ctrl uses a deterministic mock step from one leg's press to release.
- Entity Ctrl consumes the existing STATUS fields: `appliedLegMask` and the
  four-bit KW10 `contactMask`.
- An entity step requires accepted application, observed departure from contact,
  command release, and subsequent contact return. Debounce and bounded attempt
  timeouts prevent repeated contact edges from creating extra steps.
- Formal scores require at least three valid steps. Fewer steps are displayed as
  trial data rather than a precise score.
- Speed is currently weighted as 70% cadence, 20% continuity, and 10% valid-step
  count. Coordination is 40% completion rate, 25% firmware acceptance rate,
  20% rhythm stability, and 15% four-leg participation.
- A fall remains a hard failure with both scores equal to zero.
- This candidate changes only Android's read-only gameplay observer. The BLE
  control packet, ESP32 FSM, servo behavior, and safety gate are unchanged.

## Verification

Android `testDebugUnitTest` passes with 60 tests and 0 failures. Regression
coverage includes provisional scoring for one step, rejection without a valid
step, and the full entity sequence:

```text
applied → contact departure → release → contact return
```

`assembleDebug` and `git diff --check` pass. The APK is
`dist/niulai-debug-score-valid-steps-v2.apk` with
SHA-256
`0e0f3532408ffd384ab7640147c19b5f7287843d5d9cc23ba3c9529b292b8c34`.

## Open validation

The timing windows and score weights remain Testing until STATUS traces from
the real ARD-1 runtime show that applied-mask, KW10 contact, and user release
events arrive in the assumed order. If that order is not reliable, the next
step is a small firmware telemetry extension exposing per-leg FSM state and a
step identifier; it must not be inferred from a button press alone.
