# EXP-041: BLE HOME 状态 14 与执行器接受窗口修复

Status: Testing

Risk: HIGH RISK（涉及执行器 admission 与 HOME 边界；本轮实体验证只发送中性控制和 HOME，未发送非零腿掩码）

Date: 2026-09-17

## Objective

确定 Android 发送 HOME 时 `write failed: 14` 的来源，并验证：

1. 执行器接受窗口从 10 s 改为 1 s，但安全门、加密链路、中性条件和 `/OE` 停止边界仍保留；
2. HOME 请求不会因为到达时远端腿掩码尚未瞬时归零而被固件直接拒绝；
3. Android 普通 GATT 写入失败不会主动关闭连接，且 HOME 前的中性写有足够的完成等待时间。

## Environment and source

- Workspace: `firmware-esp32`
- Branch: `fw/esp32-layered-runtime`
- ESP-IDF: v5.5.5
- Target: ESP32-S3
- Firmware source: `firmware-esp32/`
- Android source: `android-controller/`
- Safety authorization: Operator confirmed mechanical fixation, personnel clearance,
  6 V/current boundary, emergency stop, and actuator condition before the bounded test.

## Diagnosis

The firmware HOME GATT callback returned `BLE_ATT_ERR_UNLIKELY` (`0x0E`) when the
authenticated request arrived while the current remote leg mask was non-neutral or
the HOME request was already pending. Android surfaced that ATT result as status 14.
The Android callback then treated an ordinary CONTROL/HOME write error as a connection
failure and closed GATT. In addition, `requestHome()` waited only 200 ms after the
neutral write, which could race a platform GATT write completion.

The fix is:

- latch HOME after secure-link validation when HOME is not already processing;
- consume the latch only after a neutral control packet arrives;
- allow a neutral control packet while HOME is pending, while rejecting non-neutral
  control packets;
- wait 2 s for the neutral write and keep the GATT connection open on ordinary write
  errors, with an operation/status log;
- set the firmware execution acceptance window to 1 s (10 control samples), not remove
  the safety gate.

## Build evidence

Fresh build command used the ESP-IDF v5.5.5 Python environment and ESP32-S3 target.

- Build directory: `firmware-esp32/build-home-fix-current-2`
- Application SHA-256:
  `6b5df132d9466c29537817df87a846ae6c7c25915b9b8a0489960c59078b1bdd`
- `sdkconfig` generated value: `CONFIG_NIULAI_ACTUATION_ACCEPTANCE_WINDOW_S=1`
- `flasher_args.json` offsets: `0x0` bootloader, `0x8000` partition table,
  `0x10000` application
- Android unit tests: `./gradlew :app:testDebugUnitTest` — BUILD SUCCESSFUL
- Android APK build: `./gradlew :app:assembleDebug` — BUILD SUCCESSFUL
- APK SHA-256:
  `3c417a16db4fd841f6e1382a9a0c3480556efa613c8f97b5b2842134c63c2a0b`
- Firmware host suite: `106 checks` — PASS
- Offline checks and `git diff --check` — PASS

## Flash and bounded hardware validation

Native Windows PowerShell used `python.exe`
with esptool v4.12.0 on `COM4`. All three metadata-selected regions returned:

```text
Hash of data verified.
Hash of data verified.
Hash of data verified.
```

The read-only status after reset was:

```text
QW_STATUS stage=ACTUATION_ENABLED fsm=SUPPORT acceptance=0/10 armed=0 failsafe=0
```

The first Windows Central probe was rejected by a stale Windows bond (`GAP disconnect
reason=531`) before any characteristic write. After removing only the Windows
`牛来快跑` bond (`UNPAIR_STATUS=0`), the same bounded probe succeeded:

```text
BLE_CONNECTED=True
NEUTRAL_WRITE=OK
HOME_WRITE=OK
UART FSM_RESET state=SUPPORT
UART BLE_HOME_REQUEST accepted=1
```

The probe sent a neutral control packet and HOME only; it sent no non-zero leg mask,
BLE motion action, `HOME` with active legs, or other movement command. No reboot or
stack overflow appeared in the read-only serial capture.

## Result and remaining verification

The firmware-side cause of status 14 and the Android connection-closing behavior are
identified and covered by host/unit/build tests. The fresh firmware path was flashed
and validated through a Windows BLE Central. A real Android phone UI run with the new
APK remains required before declaring the end-to-end Android HOME fix complete.

Rollback is to restore the prior 10 s acceptance setting and previous HOME admission/
Android error handling, then repeat the same metadata-based build and bounded test.
