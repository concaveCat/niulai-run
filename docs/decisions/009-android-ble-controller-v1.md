# Decision: Android BLE controller and protocol v1

Status:
- Accepted for the BLE bring-up phase by the requesting Operator on 2026-09-04

Problem:
The project needs a phone controller that preserves simultaneous hold-to-control input while keeping transport details outside the existing leg FSM and future actuator code.

Options:
- Native Android BLE Central/GATT Client with an ESP32 BLE Peripheral/GATT Server.
- Bluetooth Classic/RFCOMM.
- Cross-platform mobile frameworks or Web Bluetooth.
- Four independent edge-event command streams rather than one complete state mask.

Decision:
Use native Android with Kotlin, Jetpack Compose, coroutines, StateFlow and Android's platform BLE APIs. Use the custom four-byte state protocol plus the runtime interaction extensions documented in [`BLE_PROTOCOL.md`](../BLE_PROTOCOL.md): encrypted bonded HOME and read/notify IMU telemetry. The ESP32-S3 side uses the separately tracked ESP-IDF + ESP-NimBLE endpoint in `firmware-esp32/`; its current hardware acceptance remains governed by the endpoint Decision.

The Android app owns one authoritative four-bit mask and transmits the complete mask on pointer-state changes plus a 100 ms heartbeat. The ESP32 software-input watchdog target is 500 ms. BLE state is an input capability and must not call servos directly.

Scope reduction:
- No Bluetooth Classic, third-party BLE library, MTU negotiation, background service, automatic reconnect framework, persistent pairing UI or visual-polish work in v1.
- No servo commands, gait changes, automatic leg selection or whole-body behavior.
- HOME is a neutral-only request consumed by the control task; it does not bypass the safety gate or directly command a servo. IMU telemetry is display-only and uses the MPU6500 dynamic-acceleration snapshot.
- ESP-IDF bootstrap and endpoint changes are tracked separately from this Android task; they are not hidden in the Android implementation.

Consequences:
- Android runtime permission handling is required for API 31+ and legacy scan location permission for API 30 and below.
- Physical end-to-end acceptance remains pending an ESP-IDF task and real phone/ESP32 hardware.
- CONTROL_STATE and HOME_COMMAND require an encrypted bonded BLE link. The current bench profile requests LE Secure Connections without MITM for no-PIN pairing; this is a HIGH RISK test configuration, and phone pairing plus end-to-end hardware acceptance remain integration tests before treating it as production-ready.
