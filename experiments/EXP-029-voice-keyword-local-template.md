# EXP-029 ESP32-S3 本地“牛来”关键词第一阶段软件验证

## Objective

验证 ESP32-S3 上的本地“牛来”关键词实验路径：纯 C 音频特征、VAD 分段、录入式
模板保存/读取、DTW 匹配、重复抑制，以及与 I²S 麦克风服务的隔离边界。

## Hypothesis

在不引入第三方语音模型的条件下，ESP32-S3 可以运行一个可替换的低资源固定模板
检测器；它足以验证采集、分段和 `NIULAI` 事件接口，但不能证明通用中文 ASR、
跨说话人识别或实际声学识别率。

## Hardware version

无实体硬件。本轮使用 ESP32-S3 目标的交叉编译配置和 host 合成 PCM/特征输入；
MS4030、扬声器、舵机电源和执行器均未连接、未运行。

## Software commit

`8898101-dirty`（共享工作区未提交 exploratory work）。

## Parameters

- 16 kHz 单声道候选 PCM；480 samples（30 ms）特征窗，160 samples（10 ms）步进；
- 9 维特征：8 个粗频带比例和 zero-crossing rate；DTW 的默认距离阈值为 60；
- VAD RMS 候选阈值 500；连续 8 个静音帧（80 ms）结束一段语音；重复冷却候选
  为 50 帧（500 ms）；
- `voice enroll` 收集 5 段同一说话人的有声语音；后续段按最近帧归一到第一段的帧数，
  对 9 维特征逐帧取平均后只保存一个 NVS 模板；
- 当前扩展为两个说话人槽位；`voice enroll 1|2` 分别更新对应模板，旧版单模板 NVS
  数据作为槽位 1 兼容读取，双模板匹配时输出命中的 speaker 编号；
- ESP32 候选 I²S：BCLK=`GPIO9`、WS=`GPIO10`、SD=`GPIO14`、32-bit slot、slot 0、
  右移 16 位；模板使用 NVS `voice_kws/template`；
- 语音模式默认关闭，启用后只输出 `VOICE_COMMAND command=NIULAI text=牛来`，
  不调用 BLE、FSM、PCA9685、舵机或动作映射。

## Test method

1. 运行 `sh firmware-esp32/tests/host/run_host_tests.sh`，检查合成特征、静音/VAD、
   前后静音分段、溢出、匹配/不匹配、无模板和 cooldown。
2. 在 `/tmp/niulai-voice-enabled` 使用 ESP-IDF v5.5.5、ESP32-S3 和临时 defaults
   `CONFIG_NIULAI_VOICE_KEYWORD_TEST=y` 构建，不烧录、不打开串口。
3. 检查默认 `sdkconfig.defaults` 仍将语音模式设为 `n`，并人工审阅 voice 分支不
   启动 BLE、FSM、PCA9685、舵机和动作路径。

## Video / logs

无实体视频或原始麦克风录音。本轮临时构建目录为 `/tmp/niulai-voice-enabled`，
未将生成物作为实验输入提交。

## Result

- host suite 通过，`test_voice_keyword` 报告 `ALL PASS (323 checks)`；同时通过纯度
  检查，核心组件未引入 ESP-IDF 头文件或符号。
- 启用语音配置的 ESP-IDF v5.5.5 构建通过，生成 `niulai.bin`；应用分区余量为
  `0xbd020`（74%）。构建包含 `voice_keyword_test.c`，并在 voice profile 下跳过
  未完成的 MP3 播放源/资源，因此未受相邻 AUDIO-001 实验影响。
- 默认配置仍为 `CONFIG_NIULAI_VOICE_KEYWORD_TEST=n`。本轮没有烧录、串口采集、
  MS4030 波形确认、真实“牛来”录入或声学正负样本，因此没有识别率结论。
- 当前 `voice enroll 1|2` 需要为指定说话人收集五段有声语音后才保存聚合“牛来”模板；
  它不验证录入内容的中文文字。这是固定说话人/相近发音的实验检测器，不是通用语音
  转文字。槽位 1 的设备端五段录入与一次识别见下方 follow-up，槽位 2 尚待烧录后实测。

### Operator five-sample enrollment follow-up (2026-09-14)

- Hardware: ESP32-S3 QFN56 rev v0.2, native Windows CH343 `COM4`, MS4030 on
  BCLK=`GPIO9`、WS=`GPIO10`、SD=`GPIO14`; no BLE/FSM/PCA9685/actuator path was started.
- Build: ESP-IDF v5.5.5 voice profile, `CONFIG_NIULAI_AUDIO_VOLUME_PERCENT=1`,
  `slot=0`/`shift=16`, 16 kHz, 32-bit stereo I²S RX, threshold 60 and VAD RMS 500;
  application image size `0x66ff0`, approximately 60% application partition free.
- The enrollment implementation adds a forced 5-second quiet interval after each
  accepted/rejected segment. One short 3-frame fragment was rejected; accepted sample
  frame counts observed were 38, 41, 49 and 37, followed by
  `VOICE_ENROLLED ... samples=5 template_frames=38`. `voice status` then reported
  `enrolled=1` and `enrollment_samples=5/5`.
- One follow-up utterance reached the match/playback path; the device reported
  `VOICE_ACTION action=play_mama result=complete` and
  `VOICE_ACTION action=resume_mic result=ready`. This is one same-speaker smoke test,
  not a recognition-rate, false-accept/reject, or cross-speaker result.

### Two-speaker firmware extension (2026-09-14, build only)

- The service now stores two templates in NVS schema version 2 and migrates the previous
  single-template blob to speaker 1. `voice enroll 2` records five samples for speaker 2;
  `voice enroll 1` can replace speaker 1. Each slot keeps the existing five-sample,
  time-normalized feature aggregation and 5-second inter-sample pause.
- Both enrolled matchers process the same segmented utterance. If both are under threshold,
  the lower DTW distance wins and only one `play_mama` action is emitted; the event includes
  `speaker=1` or `speaker=2`.
- Host suite passed with `test_voice_keyword: ALL PASS (387 checks)` and `git diff --check`.
  ESP-IDF v5.5.5 voice build passed with image size `0x67620` and about 60% app partition
  free. This revision has not been flashed; speaker 2 has not yet been enrolled or tested.

## Next step

在不接执行器的条件下完成 MS4030 VDD、L/R、I²S slot/alignment、PCM 电平和噪声底
确认；然后由至少两位说话人各录制不少于 20 条正样本和 20 条负样本，归档原始音频、
配置和固件版本，测量误报、漏报、延迟和冷却行为。根据结果决定保留录入模板还是
引入有来源的 ESP-SR 定制 WakeNet；任何 `NIULAI` 到动作的映射另建 Decision。
