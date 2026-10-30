# Decision: 本地“牛来”语音关键词

Status:
- Testing (reopened by Operator)

Last reviewed: 2026-09-13

## Problem

项目希望用户说“牛来”时，机器人能够识别一个明确的唤醒事件，并播放指定音频。当前
ESP32-S3 Carrier 已预留 MS4030 数字麦克风的 I²S 网络，但仓库没有
ESP-SR 模型包，也没有经过实物验证的麦克风数据格式、噪声底和识别阈值。

“关键词唤醒”与“大词汇量语音转文字”是不同问题。本记录只处理固定目标
`牛来`，当前被唤醒后只播放 `牛来叫妈妈.mp3`，不定义任何运动动作。

## Facts and constraints

- Carrier 的当前候选接线是 I²S `BCLK/SCK=GPIO9`、`WS=GPIO10`、
  `MS4030 SD=GPIO14`；这些引脚和 24-bit/32-bit slot 仍须实物波形确认。
- ESP-SR 的 WakeNet 使用 16 kHz、单声道、有符号 16-bit 音频特征输入，
  并支持中文唤醒词；官方列出的现成中文词不包含“牛来”。见
  [ESP-SR](https://github.com/espressif/esp-sr) 和
  [WakeNet 文档](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/README.html)。
- 自定义唤醒词需要单独的声学模型/语料流程；不能因为代码中出现了字符串
  `牛来` 就声称已经具备中文语音识别。见
  [乐鑫中文定制流程](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html)。
- 语音事件不能绕过现有 BLE、FSM、安全门或执行器边界；当前唯一允许的副作用是
  播放 `牛来叫妈妈.mp3`，并记录 `VOICE_COMMAND command=NIULAI text=牛来`。

## Options

1. **ESP-SR WakeNet 自定义模型**：最终准确率和跨说话人能力最好，但需要
   可追溯的模型包、许可证/来源和训练语料；当前环境不能凭空生成该模型。
2. **录入式模板匹配（当前 Testing）**：不引入外部依赖，在设备上录入一条
   Operator 说的“牛来”并保存在 NVS，使用低资源音频特征和 DTW 匹配。它
   能验证麦克风、采集、分段、阈值、去重和事件边界，但不是通用 ASR，跨
   说话人和强噪声表现必须实测。
3. **Android 系统 ASR**：可利用手机的中文语音服务，但麦克风和识别位置
   不在机器人本体；不作为本 Decision 的 ESP32 实现。

## Current proposal

采用选项 2 作为可复现的第一阶段实验。`voice_keyword` 组件保持纯 C、无
ESP-IDF 依赖，主机直接测试特征量化、VAD 分段、DTW 阈值、冷却窗口和
“无模板不产生事件”。ESP32 层仅负责 MS4030 I²S RX、32-bit slot 到
16-bit PCM 的候选转换、NVS 模板存取和 UART 状态/录入命令。

录入命令为 `voice enroll 1|2`：为指定说话人连续接受 5 段有声语音，按时间轴归一并
平均特征后保存为独立的 `牛来` NVS 模板；运行时同时比较已录入的两个模板，匹配任一
模板才输出带 `speaker=1|2` 的 `NIULAI`，并在独占 I²S TX 时播放
`牛来叫妈妈.mp3`。旧版单模板会自动作为说话人 1 读取。`voice clear` 删除全部模板，
缺少模板时服务保持静默。识别事件不连接 BLE、FSM 或舵机。

## Acceptance criteria

### Software gate

- host tests cover a matching enrolled sequence, a non-matching sequence,
  leading/trailing silence, duplicate suppression, invalid/no-template input and
  five-sample time-normalized template aggregation and independent two-speaker
  matcher behavior;
- ESP-IDF source compiles when the recorded ESP-IDF v5.5.5 environment is
  available; the default firmware remains `BLE_ONLY` and does not start voice;
- `voice` test mode starts no BLE, FSM, PCA9685 or actuator and keeps `/OE HIGH`;
- a matched `VOICE_COMMAND` invokes only the embedded `牛来叫妈妈.mp3` playback,
  pauses microphone RX while TX owns the shared I²S clock pins, and resumes RX after
  playback;
- every unrun hardware check is named explicitly; no UART log is treated as
  proof of acoustic recognition.

### Hardware gate (not completed by this record)

- confirm the actual MS4030 VDD, L/R selection, I²S slot/alignment and sample
  level with the microphone connected;
- record at least 20 positive utterances and 20 negative utterances from at
  least two speakers, preserving raw audio and firmware commit/configuration;
- choose the threshold and report false accepts, false rejects, latency and
  cooldown behavior; only then decide whether to replace the template backend
  with a custom WakeNet model.

## Consequences

- The first implementation is an enrolled fixed-keyword detector, not a general
  Chinese speech-to-text engine.
- `NIULAI` remains an observable capability event; in the current Testing proposal its
  only side effect is the embedded audio playback. It does not authorize movement.
- The template format is intentionally replaceable so an ESP-SR model adapter
  can be added without changing the rest of the robot control boundary.

## Result

Host and isolated firmware verification passed on 2026-09-13: the pure C suite reported
274 checks, and an ESP-IDF v5.5.5 ESP32-S3 build with
`CONFIG_NIULAI_VOICE_KEYWORD_TEST=y` linked the voice service successfully. The
voice profile was then flashed to the Operator's ESP32-S3 on native Windows `COM4`,
with all three images reporting `Hash of data verified`; boot logs confirmed the
isolated voice task and `voice status` command. `voice capture` returned the expected
2-second, 16 kHz, 32000-sample frame, so the I²S read/UART transport path is alive.
However, both the baseline and slot/alignment comparison were almost entirely zero and
had no negative or zero-crossing samples (see [EXP-031](../../experiments/EXP-031-ms4030-i2s-capture.md)).
Therefore normal MS4030 acoustic PCM is not yet confirmed. Positive/negative sample
statistics, cross-speaker performance, and the final template-versus-ESP-SR model choice
remain open; no recognition-rate or “牛来” acoustic acceptance is claimed.

## Operator decision update (2026-09-13)

Operator has reopened this Decision after the MS4030 retest produced continuous signed
samples: `31996/32000` nonzero samples, `1459` zero crossings and `RMS=14075.81` in a
2-second capture. The voice-keyword direction may therefore continue as a Testing-stage
capability. Operator now specifies that a successful `VOICE_COMMAND` shall only play
`牛来叫妈妈.mp3`; it shall not trigger movement, BLE, FSM or actuator behavior. This
mapping is implemented in the isolated voice profile but still needs an on-device
enroll/match/playback test and positive/negative utterance measurements.

## Operator verification update (2026-09-14)

The five-sample enrollment flow was rebuilt with a forced 5-second inter-sample pause and
flashed to the Operator's ESP32-S3 through native Windows `COM4`; bootloader, application,
and partition table each reported `Hash of data verified`. Host tests remained green at
`test_voice_keyword: ALL PASS (323 checks)`, and the ESP-IDF v5.5.5 voice profile built
with a 1% audio volume configuration. The device accepted five samples, rejected one
3-frame fragment, then reported `VOICE_ENROLLED ... samples=5 template_frames=38` and
`VOICE_STATUS ... enrolled=1 ... enrollment_samples=5/5`. One subsequent utterance reached
the audio path, which reported `play_mama result=complete` and `resume_mic result=ready`.
This is a single-speaker smoke test only; it does not establish recognition rate, false
accept/reject behavior, or cross-speaker performance.
