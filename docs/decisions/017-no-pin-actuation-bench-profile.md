# Decision 017: no-PIN encrypted BLE and actuation bench profile

Status: Testing

Date: 2026-09-16

Risk: HIGH RISK

## Context

The operator needs to pair the Android controller with the ESP32-S3 without
entering a passkey and to exercise the installed actuator path on a fixed,
limited bench setup. The previous BLE profile required MITM and a displayed
passkey, while the repository baseline remained `BLE_ONLY`.

## Decision

- Keep bonding and link encryption for `CONTROL_STATE` and `HOME_COMMAND`.
- Request LE Secure Connections but disable MITM and use NoInputNoOutput, so
  the Android pairing flow does not require a PIN.
- Use `sm_sec_lvl=2`, disable `sm_sc_only` for Android compatibility, and
  require `encrypted && bonded` in the firmware admission state. GATT writes
  require `WRITE_ENC`; unencrypted or unbonded writes remain rejected.
- The public `sdkconfig.defaults` remains `BLE_ONLY`. The explicitly selected
  bench profile now lives in `sdkconfig.bench.defaults` and sets
  `CONFIG_NIULAI_MODE_ACTUATION_ENABLED`. The Kconfig choice default remains
  `BLE_ONLY` for new/unconfigured builds.
- At the time of this record, keep the existing 10-second fresh-packet
  acceptance window, neutral HOME ordering, 500 ms watchdog, contact
  readiness, pulse envelopes, `/OE` stop, safe-stop latch, and
  actuator/motion fault closure unchanged. The acceptance-window timing is
  superseded for the current profile by [`Decision 018`](018-ble-home-and-acceptance-window.md);
  the other safety requirements remain in force.

## Consequences

No-PIN pairing removes MITM protection: a nearby device able to complete the
pairing can control the encrypted session. This profile is for the explicitly
confirmed fixed-leg bench test and is not a production authentication policy.
Because `sm_sc_only` is disabled, a peer without LE Secure Connections may
fall back to legacy encrypted pairing; Android hardware acceptance must record
the negotiated security mode.

The actuator is initialized in a safe state and is not armed at boot. It can
only leave `/OE` high after the existing runtime acceptance gate is satisfied.
Any failsafe, disconnect, stale packet, fault, stop latch, or failed hardware
transaction closes the gate.

## Verification

Required software evidence: host suite, offline checks, `git diff --check`,
ESP-IDF `esp32s3` build, and Android unit tests/Debug APK build. Required
hardware evidence: fresh board flash and startup logs showing encrypted-bonded
BLE mode, `ACTUATION_ENABLED`, PCA9685 safe initialization, and no reset or
stack fault; then a staged phone pairing and low-amplitude motion test with
the operator-confirmed mechanical fixation, limits, power/current boundary,
clearance, and emergency-stop procedure.

## Current evidence

The first ACTUATION_ENABLED image was flashed on COM4 but correctly refused
PCA9685 initialization because the compatibility FL mapping still targeted
CH0, whose reviewed minimum is 1400 us, while the configured lift target was
1200 us. The mapping was corrected to the carrier order `FR→CH0/1`,
`FL→CH2/3`, `RR→CH4/5`, `RL→CH6/7`; the corrected image was rebuilt and
reflashed. Startup then reported `stage=ACTUATION_ENABLED`, registered the
encrypted-write GATT characteristics, reported `MPU6500 telemetry ready
address=0x68`, and emitted no PCA9685 initialization error. A read-only UART
`status` returned `acceptance=0/100 armed=0 failsafe=0`. No phone pairing or
motion command was performed. Full evidence is recorded in
[`EXP-038`](../../experiments/EXP-038-actuation-enabled-boot-fix.md).
