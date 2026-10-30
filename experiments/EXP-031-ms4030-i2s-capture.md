# EXP-031 MS4030 I²S 原始采集连通性检查

## Objective

确认已接入的 MS4030 是否通过 ESP32-S3 的 I²S RX 输出可用的连续音频样本。
本次只检查数据路径，不要求 Operator 说话，也不进行关键词录入或识别率验收。

## Hardware version

- ESP32-S3 板卡，芯片实测为 QFN56 rev v0.2，8 MB PSRAM；Windows 原生
  CH343 串口为 `COM4`。
- MS4030 已由 Operator 接入；模块具体板号、VDD 电压和 L/R 配置未记录。
- 舵机动作未启动，语音测试分支保持 `/OE HIGH`，BLE/FSM/PCA9685 均未启动。

## Software commit

`674d9a5-dirty`（共享工作区未提交 exploratory work）。

## Parameters

- ESP-IDF v5.5.5、ESP32-S3，BCLK=`GPIO9`、WS=`GPIO10`、DIN=`GPIO14`；
  16 kHz、32-bit stereo I²S RX，每次回传 2 秒、16-bit mono PCM，共 32000 点。
- 基线：`slot=0`、`shift=16`；对照：临时 `slot=1`、`shift=8`。
- UART 命令为 `voice capture` 和待验证的 `voice raw`；`voice capture` 数据头为
  `QWVCAP1`，主机工具为 [`tools/voice_keyword_capture.py`](../tools/voice_keyword_capture.py)。

## Test method

1. 检查 `/tmp/niulai-voice-enabled/CMakeCache.txt` 和
   `flasher_args.json`，按构建元数据使用 `esptool` 写入 `0x0`、`0x8000`、
   `0x10000` 三段；未执行全片擦除。
2. 在 COM4 读取启动日志，发送 `voice status`，确认 voice task 和 UART 命令循环。
3. 发送 `voice capture`，保存 2 秒 WAV 并仅统计样本数量、零值、正负值、峰值、
   RMS 和过零点；不把原始语音提交到仓库。
4. 用 slot/alignment 对照重复一次，最后将板卡恢复为基线 `slot=0/shift=16`。

## Video / logs

- 烧录日志在本会话工具输出中；三段镜像均报告 `Hash of data verified`。
- 基线 WAV：`host temp/niulai-voice-capture.wav`。
- 对照 WAV：`host temp/niulai-voice-capture-slot1-shift8.wav`。
- 两个原始文件均留在宿主机临时目录，未复制进仓库；原始音频是否保留由 Operator 决定。

## Result

- 软件链路通过：ESP32-S3 启动、I²S RX 初始化、2 秒缓冲填充、UART 命令和
  `QWVCAP1` 数据回传均完成；启动日志确认 `BCLK=9/WS=10/DIN=14`。
- 基线 `slot=0/shift=16`：`32000` 点中 `31641` 点为零、`359` 点为正、
  `0` 点为负，`min=0`、`max=30768`、`RMS=2044.2`、过零点 `0`；非零点集中在
  最后约 0.25 秒。
- 对照 `slot=1/shift=8`：结果同类，`31641` 点为零、无负值、峰值 `30768`、
  `RMS=2046.3`、过零点 `0`。改变 slot/shift 未得到连续双极性 PCM。
- 结论：可以确认 ESP32 的 I²S 读取和回传通路在工作，但不能确认 MS4030 已经
  输出正常音频；当前数据更像 DIN 浮空/未供电/未起振，或 BCLK/WS/DIN 时序、
  L/R slot、位对齐仍不匹配。由于本次没有说话，也没有声学识别结论。

### Operator knock retest (2026-09-13)

- 按 Operator 指令，在 `slot=0/shift=16` 基线下通过宿主机
  `tools/voice_keyword_capture.py` 采集 2 秒；Operator 在采集期间敲桌 2--3 次。
  WAV 保存在宿主机临时目录：
  `host temp/niulai-voice-capture-current.wav`。
- 统计：`32000` 点，`min=0`、`max=31084`、`peak=31084`、`RMS=2049.4`、
  过零点 `0`；符号计数为负 `0`、零 `31642`、正 `358`。非零样本只集中在
  `1.803063--1.825375 s` 的约 22 ms 窗口。
- 该时间窗可能与敲桌时刻相符，但数据仍是单极性、稀疏且与此前未证明有效的
  baseline/slot 对照同类，不能据此确认“听到了”敲击；当前仍未得到连续有效的
  MS4030 PCM。下一步仍应核对 MS4030 的 VDD/GND/LR 以及 BCLK/WS/DIN 实际时序，
  再做单变量 slot/alignment 或原始帧诊断。

### Raw-frame diagnostic implementation (2026-09-13)

- 针对上述结果新增 `voice raw`：保持当前 BCLK=`GPIO9`、WS=`GPIO10`、SD=`GPIO14`，
  显式设置 Philips、`data_bit_width=32`、`slot_bit_width=32`、stereo/both slot，
  输出软件推导的 `WS=16000 Hz`、`BCLK=1024000 Hz`，以及下一批 32 个左右 slot 的
  原始 `int32_t` 十六进制值。频率字段是配置推导值，不替代 GPIO 实测。
- 修改已通过 ESP-IDF v5.5.5 voice build、`sh firmware-esp32/tests/host/run_host_tests.sh`
  （全套通过，`test_voice_keyword` 274 checks）和 Markdown/link/diff 检查；随后按
  构建元数据烧录到 COM4，三段镜像均 `Hash of data verified`。
- 烧录后 `voice raw` 输出的前 32 个 frame 中，左右 slot 均为
  `0x00000000`；因此当前仍未看到有效的原始 I²S 数据。该结果不能区分 GPIO 时钟
  线未接、MS4030 未驱动 SD、L/R 选择不符，或实际时钟未按配置输出。
- Operator 随后确认 MS4030 的实际 BCLK/WS 接线就是 GPIO9/10，因此排除了本轮固件
  使用 GPIO9/10 而硬件接 GPIO13/12 的简单时钟线错位；L/R 电平、SD 线和 GPIO9/10/14
  的实际波形仍未独立测量。

### SD-to-GND suspicion retest (2026-09-13)

- Operator 测得 MS4030 `SD` 与 `GND` 之间约 `64 kΩ`，因此没有把它判定为直接硬短；
  电阻值本身不能证明麦克风在 I²S 时钟下主动输出数据。
- 按 Operator 授权，使用临时 `/tmp/niulai-voice-retest` voice profile 编译并经
  宿主机原生 PowerShell `COM4` 烧录。配置为 BCLK=`GPIO9`、WS=`GPIO10`、SD/RX=`GPIO14`、
  16 kHz、Philips、32-bit data/32-bit slot、stereo/both slot；bootloader、app 和
  partition table 均报告 `Hash of data verified`，未执行全片擦除，未启动执行器。
- `voice raw` 回传 `32` 个 frame：左 slot 持续变化（例如首帧
  `0x12343e00`、末帧 `0x122f5600`，期间约 `0x1221da00..0x12498000`），右 slot 的
  `32` 个值均为 `0x00000000`。启动日志报告 `WS=16000 Hz`、`BCLK=1024000 Hz`、
  `data_bits=32`、`slot_bits=32`。
- 随后 `voice capture` 收到合法 `QWVCAP1` 数据头：`16000 Hz`、`32000` 点、
  `16-bit`、单声道。主机统计为 `min=-32768`、`max=32767`、`mean=294.41`、
  `RMS=14075.81`、非零 `31996/32000`、过零点 `1459`。
- 结论：本次已确认 GPIO14 收到持续变化、包含正负过零的数字样本；相较此前全零/稀疏
  单极性结果，MS4030 SD 线不再表现为被直接拉低，ESP32 I²S RX 与麦克风数字输出链路
  已通过本次诊断。右 slot 全零与左 slot 活跃相符，但未单独测量 L/R 电平，不能据此
  冻结 L/R 接法或最终位移/增益。`int16` 统计触及两端饱和值，若未来重新启用音频用途，
  仍需重新标定位对齐和电平；按当前 Decision 012，不开发麦克风玩法，也不将该诊断事件
  接入 BLE、FSM 或执行器。
- 原始烧录与串口统计日志保留在本次会话的宿主机 PowerShell 工具输出中；未把原始语音
  数据写入仓库。

### Voice-triggered audio mapping (2026-09-13)

- Operator 指定检测到“牛来”后只播放 `牛来叫妈妈.mp3`，不触发腿动作、BLE、FSM 或舵机。
- 实现将匹配事件接入 `qw_audio_player_play_mama_blocking()`。播放前释放麦克风 RX，
  使 44.1 kHz TX 独占 GPIO9/10 的 I²S 时钟；播放完成后重新初始化 16 kHz MS4030 RX。
  语音 profile 因此同时编译 MP3 解码器和嵌入资源，但仍不启动 BLE/FSM/PCA9685/执行器。
- 新增纯 C 事件判定测试后，`sh firmware-esp32/tests/host/run_host_tests.sh` 全套通过，
  `test_voice_keyword` 为 `278` checks；ESP-IDF v5.5.5 `/tmp/niulai-voice-retest`
  voice build 链接成功，`niulai.bin` 为 `0x66c20`，应用分区余量约 `60%`。
- 本次只完成源码、host/build 验证，尚未将该新映射镜像烧录到设备，也尚未执行
  `voice enroll` → 说“牛来” → 听测的端到端验证；识别率和正/负样本统计仍未完成。

## Next step

下一步在执行器断电和 `/OE HIGH` 条件下烧录该 voice profile，使用 `voice enroll` 录入
Operator 的“牛来”，再验证匹配时只播放 MP3、播放后恢复 RX，并采集正/负样本统计。
若未来要把语音事件连接到运动，必须另建 Decision；当前不允许该扩展。
