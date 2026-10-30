# EXP-030 嵌入式“牛来叫妈妈”MP3 命令播放软件验证

## Objective

验证机器收到 UART 音频指令后，从固件嵌入资源解码并异步发送
`牛来叫妈妈.mp3` 到 MAX98357A 的 I²S 播放链路；同时验证停止、别名和语音事件
边界，不连接实体执行器。

## Hypothesis

在 ESP-IDF v5.5.5/ESP32-S3 上，嵌入式 Helix MP3 解码器可以逐帧解码该资源，并由
正常固件的异步任务转换成 32-bit 立体声 I²S；`audio play` 和 `NIULAI` 能触发同一
播放 API，而不会修改 BLE/FSM/舵机路径。

## Hardware version

ESP32-S3 QFN56 rev v0.2，宿主机原生 Windows PowerShell `COM4`，USB-Enhanced-SERIAL
CH343；本轮只烧录并运行隔离 I²S 诊断 profile。未执行舵机/运动命令；MAX98357A、
扬声器的实际供电、接线、示波器波形和听感仍待 Operator 记录。

## Software commit

`674d9a5-dirty`（诊断镜像启动日志中的 App version；共享工作区未提交 exploratory
work）。

## Parameters

- 资源：`牛来叫妈妈.mp3`，文件大小 106002 bytes；MP3 为 44.1 kHz、
  立体声、192 kbps、Layer III，带 ID3v2；构建时复制到 ASCII 临时名再嵌入，避免
  Ninja depfile 的非 ASCII 文件名问题。
- 候选 I²S：MAX98357A DIN=`GPIO4`、BCLK=`GPIO9`、WS=`GPIO10`；播放器输出
  32-bit stereo，采样率从 MP3 首帧读取；默认音量为 50%。
- 指令：`audio play`、`audio stop`；播放别名为 `audio play mama`、`play`、
  `play_mama`、`play_mama_mp3`、`NIULAI`，停止别名为 `stop audio`、`stop_mama`。
- 正常固件开启异步播放器；`CONFIG_NIULAI_AUDIO_TEST` 和
  `CONFIG_NIULAI_VOICE_KEYWORD_TEST` 仍是隔离 profile。
- 诊断 profile：`CONFIG_NIULAI_I2S_SINE_TEST=y`、`CONFIG_NIULAI_AUDIO_TEST=n`、
  `CONFIG_NIULAI_VOICE_KEYWORD_TEST=n`；GPIO `BCLK=9`、`WS=10`、`DIN=4`，
  1 kHz/48 kHz/32-bit stereo，数字幅度 5%；预期 `WS=48 kHz`、`BCLK=3.072 MHz`。
- 满幅度对照 profile：使用 `sdkconfig.i2s-sine-100.defaults`，其他 I²S 参数和隔离边界
  不变，数字幅度为 100%；预期仍为 `WS=48 kHz`、`BCLK=3.072 MHz`。

## Test method

1. 运行 `sh firmware-esp32/tests/host/run_host_tests.sh`，检查所有既有 host 测试、
   音频命令解析、ID3v2 跳过、完整 MP3 解码、帧数和采样数。
2. 使用记录的 ESP-IDF v5.5.5/toolchain 环境构建默认音频 profile：

   ```bash
   cd firmware-esp32
   cmake -S . -B build-audio -G Ninja \
     -DIDF_TARGET=esp32s3 \
     -DPYTHON=/tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env/bin/python \
     -DPYTHON_DEPS_CHECKED=1 \
     -DSDKCONFIG=build-audio/sdkconfig \
     -DSDKCONFIG_DEFAULTS=sdkconfig.defaults
   cmake --build build-audio --parallel 4
   ```

3. 在现有 voice-test 配置下运行 `ninja -C build`，确认语音隔离 profile 仍可构建。
4. 使用 `sdkconfig.i2s-sine.defaults` 构建 `build-i2s-sine`，确认该 profile 不链接
   MP3 资源/播放器，不启动麦克风，并在日志字符串中声明 GPIO9/10/4 与预期时钟。
5. 使用宿主机 PowerShell/native Python esptool 4.12.0，依据
   `build-i2s-sine/flasher_args.json` 将 bootloader、app、partition table 写入 `COM4`；
   三段均须 `Hash of data verified` 并硬复位，再读取启动日志。

   ```powershell
   Set-Location '\\wsl.localhost\牛来快跑\root\牛来快跑\firmware-esp32\build-i2s-sine'
   python -m esptool --chip esp32s3 --port COM4 --baud 460800 `
     --before default_reset --after hard_reset write_flash `
     --flash_mode dio --flash_size 16MB --flash_freq 80m `
     0x0 bootloader/bootloader.bin 0x10000 niulai.bin `
   0x8000 partition_table/partition-table.bin
   ```

6. 构建并烧录满幅度对照 profile `build-i2s-sine-100`，仍依据本次构建生成的
   `flasher_args.json` 写入 `COM4`，复位后确认启动日志报告 `amplitude=100%`。

## Video / logs

无实体视频或音频日志。host 输出和构建日志为本轮软件证据；宿主机烧录日志确认
bootloader、app、partition table 三段均 `Hash of data verified`。生成物位于本地临时
`firmware-esp32/build-audio/`、`firmware-esp32/build/`、`firmware-esp32/build-i2s-sine/`
和 `firmware-esp32/build-i2s-sine-100/`；本轮 1% 音量的串口原始摘录见
[`EXP-030 1% trial log`](logs/EXP-030-2026-09-13-audio-1pct.txt)，
未作为输入提交。

## Result

- host 全套通过：既有测试全部通过；`test_audio_command` 为 `ALL PASS (13 checks)`；
  `test_mp3_decoder` 解码 `169` 帧、`389376` samples、`44100 Hz`、双声道，并报
  `ALL PASS (1031 checks)`。
- 默认音频 profile 的 ESP-IDF v5.5.5 构建通过，成功生成嵌入资源、播放器和
  `niulai.bin`；应用分区余量约 33%。
- 启用 `CONFIG_NIULAI_VOICE_KEYWORD_TEST=y` 的隔离构建也通过；当前语音测试仍
  只输出 `VOICE_COMMAND`，没有自动调用音频播放器。
- `I2S 1 kHz sine diagnostic` profile 的 ESP-IDF v5.5.5 构建通过，生成
  `build-i2s-sine/niulai.bin`（253360 bytes，应用分区余量 76%）；它使用 GPIO9/10/4，
  与 Operator 当前确认的 MAX98357A 接线一致。主应用归档只包含 `i2s_sine_test.c`，
  未包含播放器/MP3 资源字符串。
- 2026-09-13 通过宿主机 PowerShell/native esptool 4.12.0 将该 profile 烧录到 `COM4`；
  三段镜像均 `Hash of data verified` 并硬复位。启动日志确认 ESP32-S3 rev v0.2、
  `I2S sine diagnostic ... BCLK=9 WS=10 DIN=4`，以及 `WS=48000 Hz BCLK=3072000 Hz`。
- Operator 随后反馈喇叭完全无声；尚未取得示波器截图、`SD_MODE` 电平或 OUTP/OUTN
  输出观测，因此当前只能确认 ESP32 端初始化/写入路径运行，不能确认 MAX98357A
  已正确解码或驱动喇叭。
- 为便于定位断线后的问题，新增了隔离诊断 UART 控制：`tone <Hz> <amplitude>`、
  `freq`、`amp`/`volume`、`mute`、`status` 和 `help`；范围为 1–20000 Hz、0–100%。
  纯 C host 命令测试新增 26 checks 并通过；使用 5% 默认配置重新构建
  `build-i2s-sine/niulai.bin`，镜像大小 `0x3ec20`，应用分区余量 75%。随后通过
  宿主机 PowerShell/native esptool 4.12.0 将该新增控制版烧录至 `COM4`，三段均
  `Hash of data verified` 并硬复位；启动日志和 `status` 回包确认 `1000 Hz / 5%`。
  Operator 已确认此前无声原因是 LRC 杜邦飞线断路；修复后的实际听感仍待记录。
- 随后通过 UART 发送 `amp 20`，`status` 回报确认 `1000 Hz / 20%`；本次听感结果仍待
  Operator 记录。
- 2026-09-13 1% 音量设备端命令测试：首次烧录的正常固件因 UART VFS 无输入时返回
  `EOF`，旧的 `while (fgets(...))` 命令循环在启动时退出，故首次 `audio play` 没有回执。
  已改为非阻塞 `fgetc` 逐字符轮询；host 全套测试仍通过，ESP-IDF v5.5.5 重建镜像
  `/tmp/niulai-audio-1pct/niulai.bin` 大小为 `701968` bytes，配置为
  `CONFIG_NIULAI_AUDIO_VOLUME_PERCENT=1`，GPIO `BCLK=9/WS=10/DIN=4`。
  修正版通过宿主 PowerShell/native esptool 4.12.0 烧录 COM4，三段均
  `Hash of data verified`。随后发送单行 `audio play`，设备回报
  `AUDIO_COMMAND accepted=play`、`playback start ... bytes=106002` 和
  `playback complete frames=169 rate=44100 channels=2`；这确认 UART 命令、嵌入资源、
  MP3 解码和 I²S 写出完成。PowerShell 当前代码页将中文资源名显示为问号，但资源长度和
  固件嵌入符号对应 `牛来叫妈妈.mp3`。Operator 尚未提供本轮人耳音量/音质结果。

## Next step

Operator 在确认 `/OE HIGH`、执行器断电、MAX98357A 供电/电流边界、扬声器固定和
急停方式后，继续记录 `audio play` 的实际有声/失真/声道/音量/尾部结果，并补做
`audio stop`、重复播放和 `NIULAI`；同时完成 Helix 许可证复核。1% 测试已证明设备端
命令到 MP3 解码/I²S 写出的路径，但不替代 MAX98357A 输出波形和人耳听测。
若要把本地语音 `VOICE_COMMAND` 直接连到播放器，另建 Decision 并更新安全/语音
边界，不能仅凭当前别名测试视为已接通。
