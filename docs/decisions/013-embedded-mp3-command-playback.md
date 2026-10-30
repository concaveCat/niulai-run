# Decision: 嵌入式“牛来叫妈妈”MP3 命令播放

Status:
- Testing

Last reviewed: 2026-09-13

## Problem

机器需要在收到明确指令后播放仓库中的 `牛来叫妈妈.mp3`。当前固件只有通用
I²S 数据发送 bring-up，没有 MP3 解码、音频资源打包或异步命令入口。

## Facts and constraints

- 音频资源是仓库根目录的 `牛来叫妈妈.mp3`；当前文件为 44.1 kHz、立体声、
  MPEG Layer III，带 ID3v2 前缀。
- Carrier 的当前候选音频网络为 MAX98357A DIN=`GPIO4`、BCLK=`GPIO9`、
  WS=`GPIO10`；实际模块丝印、供电、SD/MODE、扬声器和声道接线仍未验收。
- BLE v1 是固定 4 字节腿输入协议，本决定不向 BLE 帧添加音频字段。
- ESP-IDF 当前工程没有已接受的 MP3 解码能力；引入的 Helix 固定点组件保留
  原始 RCSL 1.0/RPSL 1.0 许可声明，向工程外分发前必须由 Operator 完成
  许可证复核。
- 语音 Decision 012 当前允许在隔离 voice profile 中将匹配成功的 `NIULAI` 事件
  映射为唯一的音频副作用：播放 `牛来叫妈妈.mp3`；不映射到 BLE、FSM 或执行器。

## Options

1. **嵌入资源 + 固件内解码**：设备无需文件系统或 SD 卡，命令延迟和输入可控；
   代价是占用 Flash/RAM，并需要审查解码器许可证。
2. **从 SD 卡播放**：便于更换音频，但增加文件系统、插卡、挂载失败和供电/接线
   变量；当前硬件事实不足以冻结该路径。
3. **由手机或外部主机播放**：不占 ESP32 音频资源，但不能满足机器收到指令后由
   机器人自身输出声音的目标。

## Current proposal

采用选项 1 作为当前软件实现：CMake 将根目录 MP3 作为二进制资源嵌入正常固件，
`mp3_decoder` 组件提供可替换的 Helix 解码器，`audio_player` 创建异步任务并将
解码后的 16-bit PCM 转为配置的 32-bit 立体声 I²S。播放器从 MP3 首帧读取采样率，
当前文件因而使用 44.1 kHz；音量由 `CONFIG_NIULAI_AUDIO_VOLUME_PERCENT`
控制，默认 50%。

规范 UART 指令为 `audio play`，停止指令为 `audio stop`。为兼容机器侧事件，
`audio play mama`、`play`、`play_mama`、`play_mama_mp3` 和 `NIULAI` 也触发同一
播放 API；内部事件应直接调用 `qw_audio_player_play_mama()`。新的播放请求从头
开始，停止请求在下一个解码/I²S 块边界生效。语音 profile 在检测到
`VOICE_COMMAND command=NIULAI` 后，会先释放麦克风 RX 对共享 BCLK/WS 的所有权，
同步调用播放器完成一次音频播放，再重新初始化麦克风 RX；不启动 BLE、FSM 或执行器。

## Acceptance criteria

### Software gate

- host command test 验证规范指令、别名、停止和无关输入拒绝；
- host decoder test 解码完整 MP3，并验证 ID3v2 跳过、Layer III、44.1 kHz、立体声
  和帧/采样数量；
- ESP-IDF v5.5.5 的正常固件 profile 能编译播放器、解码器和嵌入资源；
- 播放命令不修改 BLE/FSM/舵机控制边界，且音频测试/语音测试 profile 保持隔离。
- 语音 profile 的匹配事件只触发嵌入 MP3 播放，并在播放后恢复麦克风采集；不触发运动。
- 提供独立的 1 kHz I²S TX 正弦波诊断 profile，固定使用当前确认的 GPIO9/10/4，
  以示波器先区分时钟、数据线和功放/喇叭层；该 profile 不启动麦克风或 MP3 路径。

### Hardware gate (not completed by this record)

- 确认 MAX98357A 的 VCC/GND、SD/MODE、L/R、DIN/BCLK/WS 和扬声器阻抗/接线；
- 在 `/OE HIGH`、执行器断电并具备限流和急停条件下，验证启动、播放、停止、重复
  播放、音量、失真、声道和结尾是否完整；
- 完成 Helix 许可适用范围复核后，决定是否允许工程外分发。

## Consequences

- 播放资源随应用镜像发布，当前 MP3 增加约 106 kB Flash 占用；更换音频需要重新
  构建固件。
- 播放器和语音映射只提供音频能力，不生成 gait、不选择腿、不改变安全门，也不把
  语音事件映射为运动动作。
- 当前音频输出格式、模块参数和实际听感仍是候选/待验收事实，不能由构建成功推断
  MAX98357A 已经有声。

## Result

2026-09-13 软件门通过：host 指令测试 13 checks、MP3 解码测试 1031 checks，
ESP-IDF v5.5.5 正常音频 profile、默认语音隔离 profile 和 GPIO9/10/4 的 I²S 正弦波
隔离 profile 均构建成功。I²S 正弦波 profile 已通过宿主机 PowerShell/native esptool
4.12.0 烧录至 `COM4`，三段均 `Hash of data verified`，启动日志确认 GPIO 和预期时钟。
Operator 反馈喇叭完全无声；MAX98357A/扬声器的实际波形、`SD_MODE` 电平、模块参数
和许可证复核仍未完成，因此本记录保持 `Testing`。

2026-09-13 Operator 指定检测到“牛来”后只播放该 MP3，不触发任何运动。语音 profile
已加入 MP3 解码器、嵌入资源和独占式阻塞播放路径：匹配时暂停 RX，播放结束后恢复
麦克风 RX；ESP-IDF v5.5.5 voice build 成功，host voice tests 为 278 checks。
该映射尚未在设备上用 `voice enroll` → 说“牛来” → 听测完整验证，因此本记录仍保持
`Testing`。

2026-09-13 1% 音量设备命令测试：正常固件最初因非阻塞 UART VFS 的空读退出 `fgets`
命令循环，修复为非阻塞 `fgetc` 轮询后重新构建、烧录 COM4。发送 `audio play` 后
设备回报 `AUDIO_COMMAND accepted=play`、`playback complete frames=169 rate=44100
channels=2`；这确认命令解析、嵌入资源、逐帧 MP3 解码和 I²S 写出。MAX98357A 电气
参数、输出波形、人耳听感和许可证复核仍未完成，故本决定保持 `Testing`。
