# EXP-043: MPU6500 Roll/Pitch fall failure path

Status: Testing (software verification complete; physical acceptance open)

Date: 2026-09-17

Owner: Project maintainer

## Question

Can the current layered runtime use MPU6500 Roll/Pitch to assert a game
failure when either absolute filtered angle exceeds 60 degrees, stop the
actuator path safely, report the state to Android, and show a short TFT failure
transition without changing the normal ARD-1 input protocol?

## Inputs and implementation

- Branch: `fw/esp32-layered-runtime`
- Workspace: `firmware-esp32`
- ESP-IDF: v5.5.5, target `esp32s3`
- MPU6500 runtime: 25-sample static Roll/Pitch baseline, filtered relative
  angles, trigger at `abs(Roll) > 60` or `abs(Pitch) > 60` for 3 valid samples.
- Recovery: both angles below 50 degrees for 25 valid samples; recovery requires
  a fresh safety-gate acceptance window and does not replay the old mask.
- BLE: STATUS bit 5 is `FALL_DETECTED`; a new flag clears the remote input mask
  and blocks/revokes the safety gate.
- Android: physical gameplay records `FALL`, scores speed and coordination as
  zero, and uses a failure-specific local/AI feedback prompt.
- TFT: one-second full-red to black transition, then normal gameplay rendering.

## Reproducible software checks

```text
sh firmware-esp32/tests/host/run_host_tests.sh
```

Result: PASS. This includes 47 fall-detector checks, Roll/Pitch math checks,
BLE STATUS flag validation, Android-side protocol model checks, and 36 display
renderer checks including red-to-black interpolation.

Using the previously successful host build path (cached Gradle 8.13, Java 17,
and the approved host build context), Android `testDebugUnitTest` passed with
56 tests and 0 failures, and `assembleDebug` passed. The resulting APK was
copied to `dist/niulai-debug-imu-fall-current.apk`
with SHA-256
`3a701eeb5c7d10afb966be658c5b64fe00fc24796666b5de45e994ee7e47040d`.

After physical-route review, Android was corrected so an entity session that
reaches `GameplayPhase.RESULT` renders the result page from the connected
`CTRL` route instead of leaving all controls disabled on the control page.
The follow-up APK is
`dist/niulai-debug-imu-fall-result-route.apk` with
SHA-256
`a53a5b9e7fbc20e406d7dbb629afdaf9318bbbf161c3ea527789210da57cc577`.
Android `testDebugUnitTest` passed with 56 tests and 0 failures, and
`assembleDebug` passed.

```text
IDF_PATH=/tmp/niulai-esp-idf-v5.5.5
IDF_TOOLS_PATH=/tmp/niulai-idf-tools
IDF_PYTHON_ENV_PATH=/tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env
ESP_ROM_ELF_DIR=/tmp/niulai-idf-tools/tools/esp-rom-elfs/20241011
PATH=/tmp/niulai-idf-tools/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin:$PATH
.../idf.py -B build-imu-fall-current set-target esp32s3
.../idf.py -B build-imu-fall-current build
```

Result: PASS. The build compiled the current workspace source, including
`main/fall_detector.c`, `components/mpu6500_driver`, `components/ard1_core`,
and `components/motion_strategy/motion_strategy_ard1.c`. The application image
is `firmware-esp32/build-imu-fall-current/niulai.bin` with SHA-256
`f30902a4ff668303247e5c77a986ec5fa5b8dda3e15743e809bad44d2822459f`.

Generated `flasher_args.json` is authoritative and specifies:

```text
0x0     bootloader/bootloader.bin
0x8000  partition_table/partition-table.bin
0x10000 niulai.bin
```

## Physical result and safety boundary

After the Operator confirmed the board was connected and approved the operation,
the current image was flashed from native Windows PowerShell to COM4 using
`esptool.py v4.12.0` and the generated `flasher_args.json`. All three regions
reported `Hash of data verified`:

```text
0x0     bootloader/bootloader.bin
0x8000  partition_table/partition-table.bin
0x10000 niulai.bin
```

The flashed application SHA-256 is
`f30902a4ff668303247e5c77a986ec5fa5b8dda3e15743e809bad44d2822459f`.
Read-only startup monitoring confirmed `ESP-IDF v5.5.5`,
`stage=ACTUATION_ENABLED`, and `runtime_imu: MPU6500 telemetry ready
address=0x68`. A read-only UART `status` query returned
`fsm=SUPPORT acceptance=0/10 armed=0 failsafe=0`; the captured window had no
reboot, stack overflow, or PCA9685 initialization exception. `QW_STATUS` does
not print an explicit OE field; `armed=0` and `motion_not_admitted`, together
with the boot safety invariant, indicate that output remained `/OE HIGH`.

No BLE control, HOME, audio, or movement command was sent. This is a flash and
safe-boot result only, not physical fall-threshold or TFT acceptance. Physical
acceptance still requires an authorized staged test with mechanical fixation,
current-limited 6 V supply, clear area, and an immediately available emergency
stop; first motion boot remains `/OE HIGH`.

## Next step

Complete Android unit tests when the Gradle environment is available, then
validate controlled Roll/Pitch threshold injection or a safe tilt fixture before
any movement test. The current COM4 flash/startup evidence does not replace
that physical acceptance.
