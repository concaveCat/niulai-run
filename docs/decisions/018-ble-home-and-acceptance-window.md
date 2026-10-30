# Decision 018: one-second BLE acceptance and HOME admission race

Status: Testing

Date: 2026-09-17

Risk: HIGH RISK

## Context

The Android control path is now delivering encrypted bonded control packets
reliably. The ten-second actuation acceptance delay is unnecessary for the
current fixed, supervised bench workflow. Separately, Android reported
`HOME_COMMAND write failed: 14` and returned to a disconnected-looking UI.

## Evidence

The ESP32 HOME GATT callback returned `BLE_ATT_ERR_UNLIKELY` when the current
remote mask was momentarily non-neutral or a HOME request was already pending.
ATT error `0x0E` is surfaced by Android as status `14`. The Android callback
also treated every normal write error as a connection failure and closed the
GATT instance. The ViewModel gave the preceding neutral write only 200 ms,
which was shorter than a possible platform GATT write completion.

## Decision

- Keep the actuation safety gate and change its default acceptance window from
  10 seconds to 1 second (10 watchdog samples at the current 100 ms cadence).
  The 500 ms watchdog, encrypted bonded link, neutral input, contact, fault,
  `/OE`, safe-stop latch and emergency-stop boundaries remain unchanged.
- In the firmware, accept and latch an authenticated HOME request when it is
  not busy. The control task consumes it only after a neutral remote mask is
  observed; a neutral CONTROL_STATE packet is allowed to complete a pending
  HOME request.
- In Android, wait up to the normal 2-second GATT write timeout for the
  neutral packet before sending HOME. A normal CONTROL_STATE/HOME write error
  is reported with its semantic operation and status name without closing an
  otherwise live GATT connection. Scan (10 s) and connection-ready (12 s)
  timeouts are unrelated and remain unchanged.

## Verification and rollback

Host BLE protocol tests, Android unit tests, and a fresh ESP-IDF v5.5.5
ESP32-S3 build must pass. Hardware acceptance still requires flashing the new
image, encrypted Android HOME testing, read-only UART monitoring and the
operator-confirmed bench safety conditions. Rollback is the previous profile
and image; do not remove the safety gate to recover a HOME failure.
