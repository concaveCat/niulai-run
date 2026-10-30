# Decision 015 — ESP32 layered runtime boundaries

Status: Preliminary Accepted (software-only)

## Context

The repository has a working hardware-independent FSM, ESP32 device drivers,
an actuator capability, a safety gate, and isolated four-leg servo experiments,
but they are not yet one explicit HAL/driver/service/control/application
architecture. The four-leg experiment must remain replaceable while game code
is developed.

## Decision

Use a selective HAL below device drivers, platform services above drivers,
hardware-independent control and motion strategies above services, and an
application/runtime orchestrator above control. Keep safety as a mandatory
cross-cutting boundary at the actuator output.

Do not create wrappers for every ESP-IDF API. Add a HAL only for an interface
that needs host fakes, deterministic fault injection, or board/device
replacement.

## Consequences

- Existing pure-C FSM and protocol cores remain reusable.
- PCA9685, ST7789, KW10, MPU6500, audio, and BLE can be migrated incrementally.
- ARD-1 is the selected normal-runtime motion strategy, still replaceable and
  not a game dependency. It emits four public `FL/FR/RL/RR` joint targets;
  the PCA9685 adapter owns calibrated channel order, signs, rest pulses, the
  reviewed per-channel pulse envelope, and the bounded eight-channel
  transaction.
- The current isolated `servo-zero-test` remains available for bench work while
  its pure motion logic is extracted behind a strategy interface.
- The Kconfig choice default `BLE_ONLY` and `/OE HIGH` invariant remain
  unchanged. Decision 017 records the explicitly selected HIGH RISK bench
  profile `sdkconfig.bench.defaults`, which overlays `ACTUATION_ENABLED`.
- Device I/O is bounded and no longer runs under the safety-state mutex; an
  SPI timeout retains driver-owned transaction storage and fails subsequent
  writes closed until that transaction is reclaimed.
- Failed `/OE`/safe-stop/actuator operations latch a non-rearming actuator
  fault; safe-stop attempts `/OE`, rest preload, and SLEEP independently and
  retains a pending retry when any operation is unconfirmed.
- The strategy contract emits a four-leg frame and generic diagnostic
  snapshot, while the current leg FSM remains a private adapter
  implementation.
- Contact readiness and ST7789 orientation/MADCTL consistency are checked at
  the runtime boundaries.
- Explicit safe-stop is latched until a manual neutral reset; in-flight opening
  is aborted when authorization is withdrawn, and encrypted bonded BLE is a
  prerequisite for control/actuation authorization. Revocations are
  serialized with actuator I/O and an authorization epoch prevents an
  in-flight frame from being committed after revocation.
- Motion progress/NaN and PCA9685 pulse bounds are rejected before hardware I/O;
  timed-out SPI transactions never retain stack-owned descriptors or buffers.
- The normal runtime owns one shared I²C bus for PCA9685/MPU6500 and one
  serialized I²S clock-domain owner for MS4030/MAX98357A. IMU data is
  telemetry-only; voice/audio never changes motion state.
- The normal runtime selects the fixed 320×170 HUD profile and starts the
  runtime capability services without weakening the Kconfig BLE_ONLY default.

## Acceptance evidence

Evidence retained from the isolated migration and review-fix branches:

- pure HAL interfaces and host fakes are present under `components/qw_hal`;
- ESP-IDF adapters are present under `components/qw_hal_espidf`;
- PCA9685, KW10, ST7789 and MPU6500 transport paths use the adapter;
- `display_service` owns both portrait and landscape panel framing;
- `motion_strategy` contains the formal ARD-1 strategy and retains the current
  leg FSM as a compatibility/isolated-test strategy;
- the ARD-1 host regression covers two-edge acceptance, atomic rejection of a
  third rising edge, release/search/contact progression, commit-after-output,
  and calibrated public-to-device pulse bounds;
- the normal application wires shared I²C, shared I²S ownership, telemetry-only
  IMU dynamic-acceleration snapshot (with a short static gravity-baseline
  calibration), landscape HUD, and always-on voice/audio sequencing;
- the existing host suite passes, including HAL and motion-strategy tests.
- independent ESP-IDF v5.5.5 `esp32s3` builds completed; the current default
  application is `0x41250` with `0xBEDB0` (75%) free;
- the pure safety policy test covers every arm prerequisite and the runtime
  safety gate delegates its authorization predicate to that policy.
- host and ESP-IDF builds cover the encrypted-bonded BLE state, stop latch,
  bounded motion/PCA configuration and SPI lifetime fixes; Android now waits
  for bonding and an encrypted write probe. No physical secure pairing or
  actuator movement was performed in this software slice.
- bounded device transaction and explicit HAL-binding seams are present for
  PCA9685, ST7789, MPU6500 and KW10; no hardware fault injection or physical
  movement was performed in this software slice.
- the safety remediation host regressions cover the reviewed PCA9685 channel
  boundaries and reject Kconfig overrides that attempt to widen them; the
  ESP-IDF build covers the independent safe-stop and authorization-epoch
  implementation. No physical fault injection or actuator movement was
  performed.

## Preliminary acceptance

The earlier 2026-09-15 software snapshot for review fixes 1–5 was committed
as `30934e7`; its clean-worktree evidence included the offline checks, host
suite, `git diff --check`, and the then-current ESP-IDF image `0xaccd0` with
32% free application partition space. The subsequent implementation slice is
kept in the current worktree until the Operator reviews and integrates it.

This is a software-only preliminary acceptance, not final project or hardware
acceptance. Android real-device pairing, physical display orientation and
visual verification, audio listening, actuator movement, power/current/limit/
estop bench verification, and UART/leg-mask findings remain open. Findings
6–8 now have a software implementation and automated build evidence in the
current worktree, but still require non-author human review, fault injection,
and staged hardware/limit/estop verification. The implementation worktree was
not flashed and no physical movement was performed for this acceptance.
