# Decision 016: BLE runtime interaction contract

Status: Testing

Date: 2026-09-16

## Context

The current physical endpoint is an ESP32-S3 with an MPU6500 and the
four-leg KW10 contact inputs. The Android controller must operate that
endpoint without changing the accepted four-byte leg-mask transport or
allowing a GATT callback to run motion-control work directly.

## Decision

- Keep `CONTROL_STATE` and `STATUS` v1 unchanged and four bytes long.
- Add `HOME_COMMAND` (`...1003`) as an encrypted bonded Write With Response
  characteristic. Its two-byte packet is `[version=0x01][command=0x01]`.
  The firmware only queues the request; the control task consumes it after a
  neutral remote mask is observed and invokes the existing software reset
  boundary.
- Add `IMU_TELEMETRY` (`...1004`) as a Read | Notify characteristic. Its
  eight-byte packet reports the MPU6500 gravity-removed scalar dynamic
  acceleration in centi-g, sample age in milliseconds, validity/clipping
  flags, and a telemetry sequence.
- Keep IMU sampling/state refresh at the 100 Hz control cadence, but publish
  the latest telemetry value at 20 Hz (50 ms period) so BLE notification work
  cannot flood the control task or UART.
- Android waits for the completion callback of encrypted bonded characteristic
  writes. A successful start of a platform GATT write is not treated as
  delivery success.
- The Android control page shows the dynamic acceleration in a bottom status
  bar. The current UI thresholds are normal up to 0.25 g, warning above 0.25 g,
  and high above 0.60 g; invalid, stale, or unavailable data is gray.

## Safety boundary

The home request requires an encrypted bonded link, does not carry joint targets, and is
rejected by the control task unless the remote mask is neutral. It does not
bypass the safety gate. Physical actuation remains governed by the existing
stage, acceptance window, failsafe, contact, actuator, and motion-fault
conditions. Decision 017 records the current no-PIN/high-risk bench profile
and the explicit `ACTUATION_ENABLED` build selection; it does not weaken any
runtime failsafe or emergency-stop path.

## Verification

The pure C and Android codecs must reject wrong lengths, versions, reserved
bits, and invalid command values. Android tests cover telemetry decoding,
home ordering after neutralization, and the existing failsafe/disconnect
paths. The ESP-IDF build and the real ESP32/phone pairing test remain
required before declaring radio interoperability accepted.
