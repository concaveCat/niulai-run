# 牛来快跑 BLE GATT Protocol v1

Status: **Accepted interface for Android/ESP32-S3 BLE bring-up; actuation remains gated by the runtime safety path**

Last reviewed: 2026-09-17

## 1. Scope and roles

The Android controller is the BLE Central and GATT Client. The target `ESP32-S3-WROOM-1-N16R8` is the BLE Peripheral and GATT Server, implemented in `firmware-esp32/` with ESP-IDF and ESP-NimBLE.

This boundary transports the player's complete four-leg hold state. It does not define joint targets, servo commands, gait planning, automatic leg selection, compensating steps or whole-body balance. A received mask must first become a replaceable software input capability; it must not be wired directly to actuators.

The ESP32 endpoint preserves the existing hardware-independent control layer: contact telemetry is a status boundary and does not authorize actuator motion.

## 2. GATT service

| Item | UUID | Properties |
| --- | --- | --- |
| 牛来快跑 control service | `7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11` | Primary service |
| `CONTROL_STATE` | `7d4e1001-8a52-4d9e-b0f8-57e6c59e2e11` | Encrypted bonded Write with Response (`WRITE_ENC`) |
| `STATUS` | `7d4e1002-8a52-4d9e-b0f8-57e6c59e2e11` | Read, Notify |
| `HOME_COMMAND` | `7d4e1003-8a52-4d9e-b0f8-57e6c59e2e11` | Encrypted bonded Write with Response (`WRITE_ENC`) |
| `IMU_TELEMETRY` | `7d4e1004-8a52-4d9e-b0f8-57e6c59e2e11` | Read, Notify |
| `GAME_MODE` | `7d4e1005-8a52-4d9e-b0f8-57e6c59e2e11` | Encrypted bonded Write with Response (`WRITE_ENC`) |
| `AUDIO_COMMAND` | `7d4e1006-8a52-4d9e-b0f8-57e6c59e2e11` | Encrypted bonded Write with Response (`WRITE_ENC`) |

The ESP32 must advertise the complete 128-bit service UUID. Android accepts scan results only when the advertised service UUID or the exact `牛来快跑` name identifies a candidate, because some Android stacks omit a service UUID delivered in the Scan Response when a platform-side filter is used. It then rejects a connected device if the service or any of the five required runtime characteristic/property sets is missing. `GAME_MODE` remains optional for connection readiness because it only changes the TFT scene. Android does not expose the connection as ready until STATUS notification subscription, IMU telemetry notification subscription, and the initial STATUS read all complete successfully. Notifications use the standard Client Characteristic Configuration Descriptor UUID `00002902-0000-1000-8000-00805f9b34fb`.

The Android client bounds the discovery phase with a 10-second scan timeout and the GATT connection/service/STATUS-ready phase with a 12-second timeout. A timeout, malformed STATUS or IMU packet, or disconnect closes the active GATT instance and returns to a retryable disconnected state; a permission failure reports an error and prevents the affected operation. Callbacks from an older GATT instance are ignored and cannot clear characteristics belonging to a newer connection.

No MTU negotiation is needed: the largest application packet is eight bytes.

`GAME_MODE` is an independent, display-only state. It is exactly two bytes,
`[version=0x01][mode]`, where mode `0x00` selects the single-player TFT
scene and mode `0x01` selects the multiplayer TFT scene. It is deliberately
not part of `CONTROL_STATE`, `STATUS`, the watchdog, the FSM, the actuator
path or the safety gate. The ESP32 defaults to single-player display mode on
boot and rejects values other than `0x00` and `0x01`.

## 3. Leg bit mask

| Bit | Value | Leg |
| --- | --- | --- |
| 0 | `0x01` | Front left (`FL`) |
| 1 | `0x02` | Front right (`FR`) |
| 2 | `0x04` | Rear left (`RL`) |
| 3 | `0x08` | Rear right (`RR`) |
| 4..7 | — | Reserved; must be zero |

The mask is level state, not four independent edge streams. Every state-change packet contains the complete current mask.

## 4. `CONTROL_PACKET_V1`

Exactly four bytes:

| Byte | Field | Rule |
| --- | --- | --- |
| 0 | Version | `0x01` |
| 1 | Sequence | Unsigned 8-bit value |
| 2 | Leg mask | Bits 0..3 as above; bits 4..7 zero |
| 3 | Flags/reserved | `0x00` |

The Android sequence starts at zero for a fresh process and increments modulo 256 for each accepted pointer DOWN, UP or CANCEL that changes tracked pointer state, and for an explicit best-effort neutralization before lifecycle loss/disconnect. A 100 ms heartbeat repeats the latest complete packet with the same sequence number; it does not create synthetic input transitions.

The receiver validates exact length, version, reserved mask bits and flags before applying a packet. Valid duplicate-sequence heartbeats refresh `last_rx_time` and the failsafe watchdog. V1 sequence numbers are observability/acknowledgement metadata only: they do not reject duplicates, impose cross-reconnect ordering or provide replay protection.

Example state sequence:

```text
initial             mask=0x00
FL DOWN             mask=0x01
RR DOWN             mask=0x09
FL UP               mask=0x08
RR UP               mask=0x00
```

Control writes use Write With Response. The Android client does not expose the
connection as `CONNECTED` until an encrypted bonded neutral control probe receives
a successful GATT write callback. This prevents a connected-but-unauthorized
link from being presented as gameplay-ready. Normal control and home writes
also wait for their GATT write callbacks; starting a platform write is not
treated as delivery success.

## 5. `STATUS_PACKET_V1`

Exactly four bytes:

| Byte | Field | Rule |
| --- | --- | --- |
| 0 | Version | `0x01` |
| 1 | Last accepted sequence | Sequence from the latest valid control packet |
| 2 | Applied leg mask | Current software input mask; bits 4..7 zero |
| 3 | Status flags | Bit 0 `FAILSAFE_ACTIVE`; bits 1..4 debounced KW10 contact mask (FL/FR/RL/RR); bit 5 `FALL_DETECTED`; bit 6 `HOME_COMPLETE`; bit 7 zero |

The contact mask is encoded as `(flags >> 1) & 0x0F`. A set contact bit means
the corresponding normalized KW10 input has remained pressed for the configured
debounce window. The current electrical defaults are uniform: FL/FR/RL/RR are
active-high; firmware applies that conversion before encoding the semantic mask.
The Android UI presents this accepted contact state as
`完美触碰` and may detect a rising bit as a touchdown event. This is a software
label for the current prototype hypothesis; it does not prove mechanical
contact quality while the KW10 electrical and mechanical behavior remains
unverified.

The physical-to-semantic mapping is owned by the firmware configuration, not by
Android: the current defaults map GPIO39 to FR, GPIO40 to FL, GPIO41 to RR, and
GPIO42 to RL. Android renders the corresponding FL/FR/RL/RR button from the
semantic contact bit it receives. The installed polarity is uniform:
FL/FR/RL/RR are active-high; firmware normalizes this before producing the
semantic mask.

`FAILSAFE_ACTIVE=1` means the receiver forced its software input mask to zero because no valid control packet arrived within the watchdog interval. It does not claim a physical power cutoff or servo-safe state. A valid packet clears the flag and updates the software input state. The server should notify STATUS after each accepted control packet, whenever failsafe state changes, and whenever the debounced KW10 contact mask changes; STATUS also remains readable.

`HOME_COMPLETE=1` means that the control task has accepted the calibrated
all-legs formal-rest frame after an authenticated `HOME_COMMAND`. It is not
set merely when the GATT write is acknowledged. A subsequent non-neutral
control packet clears the bit. Android uses this bit as the preferred settle
confirmation before ending an entity gameplay session; a bounded timeout still
falls back to the firmware disconnect safety stop if a notification is lost.

`FALL_DETECTED=1` means the runtime MPU6500 posture detector has observed the
absolute filtered Roll or Pitch above 60 degrees for three consecutive valid
samples. The firmware clears the remote input mask, prevents the safety gate
from opening, and forces the actuator stop path to keep `/OE` HIGH. The flag
clears only after both angles remain below 50 degrees for 25 valid samples;
clearing it does not re-arm the gate or replay an old input. Android treats a
new `FALL_DETECTED` status as a physical gameplay failure and records both
game scores as zero. The server notifies STATUS when this state changes.

## 6. Heartbeat, disconnect and failsafe

- While the controller screen is active and GATT setup is complete, Android retransmits the latest packet every approximately 100 ms.
- The ESP32 watchdog target is 500 ms from the latest valid packet. Expiry sets the remote input mask to zero and sets `FAILSAFE_ACTIVE`.
- Android clears all tracked pointers on pointer cancellation, activity/lifecycle loss, leaving the controller screen and BLE disconnection.
- If GATT is still connected, Android makes a best-effort zero-mask write before disconnect/background transition. BLE delivery is not guaranteed, so the ESP32 watchdog remains mandatory.
- Reconnection begins from a local zero mask. V1 does not require background auto-reconnect; the app returns to discovery and permits a clean manual reconnect.

## 7. Runtime interaction packets

### `HOME_COMMAND_V1`

`HOME_COMMAND` is exactly two bytes: `[version=0x01][command=0x01]`. It uses an
encrypted bonded link and Write With Response. Android clears all local holds,
waits for the neutral `CONTROL_PACKET_V1` write to complete, and only then
sends this command. The ESP32 GATT callback validates and latches the
authenticated request; the 100 Hz control task consumes it only when the
encrypted bonded remote mask is neutral. A neutral control packet is still
accepted while HOME is pending so a small write-order race cannot deadlock the
neutral-only gate. It then invokes the existing software reset boundary. HOME
carries no joint target and does not bypass the safety gate.

When the Android user ends an entity gameplay session, the app stops its
heartbeat, waits for a neutral control write, sends `HOME_COMMAND`, waits up to
1.5 seconds for `HOME_COMPLETE`, and then disconnects. The disconnect is
intentional: the firmware disconnect hook closes `/OE` and runs the PCA9685
safe-stop sequence, so the result page is kept visible without leaving the
servos energized. A failed HOME or missing completion notification still takes
the same bounded disconnect path.

### `IMU_TELEMETRY_V1`

`IMU_TELEMETRY` is exactly eight bytes, little-endian for multi-byte fields:

| Byte(s) | Field | Rule |
| --- | --- | --- |
| 0 | Version | `0x01` |
| 1 | Flags | bit 0 valid, bit 1 clipped, bit 2 stale; bits 3..7 zero |
| 2..3 | Dynamic acceleration | unsigned centi-g, gravity removed |
| 4..5 | Sample age | unsigned milliseconds, capped at 65535 |
| 6..7 | Telemetry sequence | unsigned 16-bit counter |

The value is the MPU6500 runtime's scalar dynamic-acceleration proxy, not raw
`1 g` magnitude. Android renders it in the control page bottom bar. The current
display policy is green through 0.25 g, yellow above 0.25 g, red above 0.60 g
or when the sensor is clipped, and gray while invalid or stale.
The firmware refreshes the underlying snapshot at the 100 Hz control cadence
but sends the latest telemetry packet to BLE at 20 Hz (one notification every
50 ms); a read always returns the latest snapshot.

### `AUDIO_COMMAND_V1`

`AUDIO_COMMAND` is exactly two bytes: `[version=0x01][command=0x01]`.
Command `0x01` is `PLAY_MAMA`; it is accepted only on the encrypted bonded
link and queues the ESP32's embedded `牛来叫妈妈.mp3` for asynchronous playback.
The GATT callback only enqueues the existing voice/audio action and never calls
the control task, FSM, safety gate or actuator path. Android's connected Ctrl
page sends this command when the `牛来` button is tapped; the Test Ctrl page
does not claim to drive a machine.

## 8. ESP32 software boundary

The ESP32 endpoint exposes a representation equivalent to:

```c
typedef struct {
    uint8_t sequence;
    uint8_t leg_mask;
    uint8_t contact_mask;
    int64_t last_rx_time_us;
    bool failsafe_active;
} RemoteLegInput;
```

For each valid packet it should log, for example:

```text
BLE_CTRL_RX seq=52 mask=0x09 FL=1 FR=0 RL=0 RR=1
```

This value is an input adapter boundary only. Servo actuation remains explicitly out of scope.

## 9. Validation and security boundary

Automated Android tests cover packet encode/decode, HOME, IMU telemetry and AUDIO_COMMAND validation, reserved-bit rejection, mask transitions, cancellation, disconnect clearing and sequence wrap. The fake-BLE ViewModel state-flow tests additionally cover the initial neutral packet, local input, HOME neutralization ordering, input suppression during HOME, FAILSAFE-driven neutralization, disconnect-before-drop neutralization and lifecycle deactivation without a heartbeat leak. The firmware host suite covers the four-byte layout, HOME/CONTROL admission gates, STATUS/IMU packet layout, AUDIO_COMMAND validation, repeated heartbeats, exact 500 ms FAILSAFE, recovery and disconnect clearing. A real ESP32/phone is still required to validate radio discovery, encrypted bonded GATT interoperability, notification timing, Android vendor behavior, HOME execution, machine-side audio output and the 500 ms hardware-side watchdog.

The current bring-up profile keeps bonding and encryption, requests LE Secure
Connections, and deliberately disables MITM so Android can pair without a PIN.
`sm_sc_only` is disabled for Android compatibility, so a peer that cannot do
LE Secure Connections may fall back to legacy encrypted pairing; this is a
HIGH RISK test profile and must not be treated as production authentication.
The GATT control and home characteristics require encryption, and the firmware
also requires the link to be bonded before accepting either write. Android
requests or waits for bonding, then performs the encrypted bonded neutral
control probe described above. The control characteristic and safety gate still
reject unencrypted or unbonded access. Actual phone/board pairing and secure
link acceptance remain hardware/integration tests.
