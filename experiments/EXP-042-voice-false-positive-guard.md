# EXP-042: “牛来”负样本误报与 I²S 质量门修复验证

## Status

Testing. The current-source image was flashed to native Windows `COM4` on
2026-09-17; the required repeated positive/negative voice acceptance run is
still pending.

## Scope and acceptance criteria

- Observe the normal ESP32 runtime over native Windows `COM4` without sending
  motion, HOME, enrollment, or playback commands.
- Record whether a negative utterance causes a `VOICE_COMMAND` event.
- Reject the observed no-speech distance-50 candidate while preserving an exact
  template match in the dependency-free matcher tests.
- Keep the microphone/audio side effect isolated from BLE, FSM, PCA9685 and
  actuators.

## Observation

On 2026-09-17, a DTR/RTS-disabled, read-only 60-second COM4 listener captured:

```text
VOICE_COMMAND speaker=1 command=NIULAI text=... distance=50 frames=28
VOICE_ACTION action=play_mama begin=1
```

The Operator confirmed they had not said “牛来”. This is a confirmed negative
sample for this run. The event identifies speaker 1; prior records contain only
speaker-1 enrollment, and speaker 2 enrollment was not completed. A subsequent
`voice status` query was harmless but returned `APP_MAIN unknown_command` because
the normal runtime exposes only the audio command seam; it did not alter NVS.

Historical `EXP-031` captures also contained a known single-polarity/zero-
crossing failure mode, so high RMS alone is not sufficient evidence of speech.

## Source change

The current source now:

- requires `QW_VOICE_MIN_ZERO_CROSSING_RATE=2` for a frame to enter enrollment or
  matching;
- changes the default template distance threshold from 60 to 40, so the
  observed distance-50 candidate is rejected;
- logs rejected completed candidates as `VOICE_REJECT`;
- resets the matcher and ignores 100 feature frames (1 second) after playback,
  preventing speaker output/residual audio from immediately starting a new
  utterance.

## Verification

- `sh firmware-esp32/tests/host/run_host_tests.sh`: passed; `test_voice_keyword`
  reports 425 checks, including one-sided-burst rejection and distance-50
  negative regression.
- ESP-IDF v5.5.5 / ESP32-S3 fresh build directory:
  `firmware-esp32/build-voice-false-positive-current`.
- `niulai.bin` SHA-256:
  `78378d157d733e3a57b777a1e4548f6f6a3cd6479eb4247e6efbd8208728a9c4`.
- `sh tools/run_offline_checks.sh`: passed; `git diff --check`: passed.
- `flasher_args.json` selected `0x0` bootloader, `0x8000` partition table, and
  `0x10000` application; all three writes returned `Hash of data verified`.
- The flashed application is `niulai.bin` SHA-256
  `78378d157d733e3a57b777a1e4548f6f6a3cd6479eb4247e6efbd8208728a9c4`.
- A DTR/RTS-disabled, read-only 12-second listener captured stable
  `control_diag` output with `free=4764` and rejected candidates at distances
  420 and 149 with threshold 40; no `VOICE_COMMAND`, playback action, reboot,
  stack overflow, or PCA error appeared in the captured window. The listener
  was opened after the hard reset, so this is a runtime-window result rather
  than a complete early-boot transcript.
- No BLE, motion, HOME, enrollment, playback, or UART control command was
  sent in this experiment.

## Next step

Keep `/OE HIGH` and repeat at least 20 negative utterances plus 20 positive
“牛来” utterances. Tune the threshold only from those recorded distances; do
not claim recognition rate from the current runtime-window observation alone.
