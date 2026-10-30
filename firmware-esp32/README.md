# ESP32-S3 固件（firmware-esp32）

基于 **ESP-IDF v5.5.x + ESP-NimBLE** 的牛来快跑机器人端点。它负责：

- BLE GATT 外设与 500 ms failsafe；
- 100 Hz 控制任务与四腿 ARD-1 运动策略；
- PCA9685 八路舵机输出与安全门；
- KW10 足端触地采样/去抖；
- MPU6500 姿态遥测与摔倒检测；
- ST7789 TFT HUD；
- MAX98357A / MS4030 共享 I²S 音频链路；
- 可选的本地“牛来”关键词诊断。

> 默认构建是 **`BLE_ONLY`**：`/OE` 保持高、PCA9685 不初始化、动作只进入空执行器。实体动作必须显式使用 `sdkconfig.bench.defaults`，并在受控台架进行。

---

## 1. 目标硬件与工具链

| 项目 | 值 |
| --- | --- |
| MCU | ESP32-S3-WROOM-1-N16R8（16 MB flash / 8 MB octal PSRAM） |
| SDK | ESP-IDF v5.5.x（基线 v5.5.5） |
| BLE | ESP-NimBLE，Peripheral，单连接 |
| 控制频率 | 100 Hz |
| 舵机总线 | PCA9685，I²C `0x40`，50 Hz |
| 固件框架 | ESP-IDF + FreeRTOS + 选择性纯 C HAL |

---

## 2. 分层结构

```text
Application / app_main
  ├─ control_task        100 Hz：接触采样、IMU、FSM、安全门、执行器
  ├─ watchdog_task       100 ms：BLE 新鲜度与 failsafe
  ├─ display_hud         ST7789 只读快照
  ├─ runtime_imu         MPU6500 标定/滤波/遥测
  ├─ runtime_i2s         MS4030 / MAX98357A 时钟域
  └─ voice/audio         可选关键词与 MP3 播放
        │
Control & motion domain
  ├─ motion_strategy     可替换策略接口
  ├─ motion_strategy_ard1 ARD-1 / PastureStep-Reserve
  ├─ leg_fsm             兼容单腿策略
  ├─ control_glue        能力组合
  └─ safety_policy       纯函数 arm predicate
        │
Platform services
  ├─ i2c_bus_service     共享 I²C
  ├─ foot_contact        采样/去抖
  ├─ safety_gate         /OE、授权 epoch、safe-stop
  ├─ display_service     方向/分块/RGB565
  └─ audio/voice         播放与关键词
        │
Device drivers
  ├─ actuator_pca9685
  ├─ mpu6500_driver
  ├─ st7789_driver
  ├─ i2s_audio_driver
  └─ ble_input / ble_protocol
        │
Selective HAL
  ├─ qw_hal              纯接口 + host fake
  └─ qw_hal_espidf       ESP-IDF 适配
```

依赖规则：

1. 纯控制/协议核心不依赖 ESP-IDF 或 FreeRTOS，可在 host 上用普通 gcc 测试。
2. 驱动只向下依赖 HAL/BSP，不向上调用应用层。
3. 服务依赖驱动能力，不直接操作私有寄存器。
4. 应用依赖服务和接口，不直接依赖具体器件。
5. 物理腿序在公共接口统一为 `FL/FR/RL/RR`；PCA9685 通道顺序在 adapter 内处理。

---

## 3. 启动顺序（安全关键）

`app_main()` 的顺序是刻意设计的：

1. 打印启动 banner（stage + runtime scope）。
2. **先把 `/OE` 配置为输出并拉高**；如果这一步失败，固件停在这里，不继续初始化任何硬件。
3. 初始化 I²C 服务、足端输入、安全门、控制任务同步状态和 BLE。
4. 创建 watchdog 任务（优先级 10）和 control 任务（优先级 6，栈 8192 B）。
5. 启动 IMU、HUD、可选语音服务。
6. 进入 UART 命令循环：`status`、`transitions`、`reset`、`audio play/stop`、`voice ...`。

`/OE` 是 **motion disable**，不是电源切断。它只能由安全门在 `ACTUATION_ENABLED` 且所有条件满足时拉低。

---

## 4. 运行时档位

Kconfig 菜单 `牛来快跑 → Safety gating stage` 提供三档：

| 档位 | `/OE` | PCA9685 | 执行器 | 用途 |
| --- | --- | --- | --- | --- |
| `BLE_ONLY`（公共默认） | 始终高 | 不初始化 | null（只打日志） | BLE/显示/音频 bring-up |
| `BLE_VERIFIED` | 始终高 | 初始化为安全值并 SLEEP | null | 验证 I²C/PCA 初始化 |
| `ACTUATION_ENABLED` | 启动高；仅安全门打开时低 | 初始 SLEEP；门打开时唤醒 | PCA9685 | 受控台架动作 |

`ACTUATION_ENABLED` 的安全门只有在以下条件**全部满足**时才打开：

- 加密且已绑定的 BLE 链路；
- 连续收到新鲜控制包（间隔 < 500 ms）；
- 通过 1 s 接受窗口（默认 10 个 100 ms watchdog tick）；
- 无 `FAILSAFE`；
- 运动策略不在 `FAULT`；
- 四路触地输入已就绪；
- 无执行器故障/安全停止锁存；
- PCA9685 初始化、初始保持帧和控制任务初始化成功。

任一条件被破坏，安全门立即拉高 `/OE`、让 PCA9685 进入 SLEEP，并切回 null 执行器。强制安全停止会锁存，只有链路恢复为加密绑定且四路输入回到中立后才允许清除。

---

## 5. BLE 端点

BLE 设备名为 `牛来快跑`，同时广播完整的 128-bit 服务 UUID。GATT 服务与特征：

| 特征 | UUID | 属性 |
| --- | --- | --- |
| 服务 | `7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11` | Primary |
| `CONTROL_STATE` | `...1001...` | Encrypted bonded Write With Response |
| `STATUS` | `...1002...` | Read, Notify |
| `HOME_COMMAND` | `...1003...` | Encrypted bonded Write With Response |
| `IMU_TELEMETRY` | `...1004...` | Read, Notify |
| `GAME_MODE` | `...1005...` | Encrypted bonded Write With Response |
| `AUDIO_COMMAND` | `...1006...` | Encrypted bonded Write With Response |

控制包固定 4 字节：`[version=0x01][sequence][leg_mask][flags=0]`。腿掩码 bit 0..3 为 `FL/FR/RL/RR`。Android 约每 100 ms 重发最新完整包；固件在 500 ms 边界进入 failsafe。

完整字段、STATUS 位定义、HOME/IMU/AUDIO 包和 Android 侧行为见 [`../docs/BLE_PROTOCOL.md`](../docs/BLE_PROTOCOL.md)。

---

## 6. 运动策略

正常运行时选择 **ARD-1 / PastureStep-Reserve** 四腿策略：

| 参数 | 默认值 |
| --- | --- |
| 抬腿时长 | 300 ms |
| 摆动时长 | 360 ms |
| 寻地时长 | 120 ms |
| 名义步长 | 30 mm |
| 抬脚 Z | -60 mm |
| 触地 Z | -100 mm |

策略输出公共 `FL/FR/RL/RR` 关节目标帧，不直接操作 PCA9685。兼容单腿 `leg_fsm` 仍保留用于隔离测试和旧路径。当前**不包含**：

- 自动选择下一条腿；
- 完整 gait planner；
- 自动补步/恢复平衡；
- 全身平衡控制。

这些是明确的非目标。玩家决定何时迈哪条腿；控制器只执行有界的单腿/双腿动作。

PCA9685 adapter 内部维护：

- 公共腿序到物理通道的映射（`FR→CH0/1, FL→CH2/3, RR→CH4/5, RL→CH6/7`）；
- 校准静息脉宽；
- 每通道硬脉宽包络；
- 每帧最大 250 µs 变化；
- 原子 auto-increment 写入；
- 失败重试与 safe-stop。

> 脉宽、方向和零位是特定样机的标定候选，不是通用安全范围。换舵机/结构后必须重新标定。

---

## 7. 外设

### 7.1 KW10 足端

- 100 Hz 采样；
- 默认 2 tick（20 ms）去抖；
- 当前安装为 C-NC，固件按 active-high 归一化；
- 物理映射：`GPIO39=FR`、`GPIO40=FL`、`GPIO41=RR`、`GPIO42=RL`；
- 对外语义掩码：bit 0..3 = `FL/FR/RL/RR`；
- 去抖后的掩码通过 STATUS flags bit 1..4 上报 Android。

去抖时长和机械触地方向仍需台架确认；`完美触碰` 是软件标签，不是机械验收。

### 7.2 MPU6500

- I²C 地址 `0x68`，`WHO_AM_I=0x70`；
- 100 Hz 控制节拍刷新快照；
- 启动后 25 个静态样本建立 Roll/Pitch 重力基线；
- 任一绝对滤波角 > 60° 连续 3 个有效样本 → `FALL_DETECTED`；
- 两轴均 < 50° 连续 25 个样本才清除；
- 清除后不会重放旧输入，必须重新经过安全门接纳；
- 摔倒会清空远端掩码、保持 `/OE` 高，并通过 STATUS bit 5 通知 Android。

### 7.3 ST7789 HUD

- 默认 170×320 竖向；可选 320×170 横向；
- RGB565；
- 显示等待连接、单人、双人场景；
- 动作腿在对应角落叠加爪子图标；
- 顶部 BLE/运行时间，中部腿状态，底部动态加速度和 `/OE`；
- 显示只读快照，不能授权动作。

### 7.4 I²S 音频

- MAX98357A：`DIN=GPIO4`、`BCLK=GPIO9`、`WS=GPIO10`；
- MS4030：`SD=GPIO14`，与功放共享 BCLK/WS；
- `runtime_i2s` 串行化麦克风 RX 和播放 TX；
- 内置 `牛来叫妈妈.mp3` 由 Helix 定点 MP3 解码器逐帧解码；
- UART：`audio play`、`audio stop`；
- BLE：`AUDIO_COMMAND` 的 `PLAY_MAMA`；
- 播放与运动路径隔离：音频不会改变控制包、FSM、安全门或舵机。

### 7.5 可选语音诊断

隔离 profile 支持本地模板式“牛来”检测：

```text
voice enroll [1|2]   # 每个说话人录入 5 段
voice status
voice capture        # 2 s PCM 经 UART 回传
voice raw            # 原始 I²S slot
voice clear
```

它是固定说话人的低资源模板匹配器，不是通用中文 ASR。默认关闭，不接入玩法。

---

## 8. 构建与烧录

### 8.1 默认安全构建（BLE_ONLY）

```bash
cd firmware-esp32
idf.py set-target esp32s3
idf.py build
```

产物：

```text
build/niulai.bin
build/flasher_args.json
```

烧录：

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Windows 使用 `COMx`。烧录偏移以本次 `flasher_args.json` 为准；当前单 app 布局通常为：

```text
bootloader      0x0
partition table 0x8000
application     0x10000
```

### 8.2 台架动作构建（HIGH RISK）

```bash
cd firmware-esp32
idf.py -B build-bench \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bench.defaults" \
  set-target esp32s3 build
```

烧录前必须满足根目录 README 的[安全边界](../README.md#安全边界)：

- 机械固定、人员净空；
- 10 A 保险；
- 6 V 限流；
- 可立即断开 XT30；
- 八个关节的通道/方向/限位已核对；
- 首次动作从低幅度、单腿、短时开始。

### 8.3 隔离 bring-up profile

| Profile | 作用 | 构建方式 |
| --- | --- | --- |
| `sdkconfig.hud-landscape.defaults` | 320×170 HUD 预览 | `-D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.hud-landscape.defaults"` |
| `sdkconfig.audio-listen.defaults` | 10% 音量听测 | 同上，替换文件名 |
| `sdkconfig.i2s-sine.defaults` | 1 kHz I²S 正弦 | 同上 |
| `sdkconfig.i2s-sine-100.defaults` | 100% 数字幅度正弦 | 同上 |
| `NIULAI_DISPLAY_TEST` | ST7789 彩条 | `menuconfig` 开启 |
| `NIULAI_DISPLAY_HUD_TEST` | HUD 循环预览 | `menuconfig` 开启 |
| `NIULAI_AUDIO_TEST` | 《欢乐颂》旋律 | `menuconfig` 开启 |
| `NIULAI_I2S_LOOPBACK_TEST` | GPIO4→GPIO14 回环 | `menuconfig` 开启 |
| `NIULAI_MPU6500_PROBE_TEST` | MPU6500 只读探测 | `menuconfig` 开启 |
| `NIULAI_FOOT_CONTACT_TEST` | KW10 去抖/极性 | `menuconfig` 开启 |
| `NIULAI_GPIO39_CLI` | 单针按钮 CLI | `menuconfig` 开启 |
| `NIULAI_VOICE_KEYWORD_TEST` | 本地关键词诊断 | `menuconfig` 开启 |

所有隔离 profile 都保持 `/OE` 高，并跳过 BLE/FSM/执行器（除非明确说明）。

---

## 9. 测试

### 9.1 纯 C host suite

```bash
cd firmware-esp32
tests/host/run_host_tests.sh
```

覆盖：

- BLE 包校验、STATUS/IMU/HOME/AUDIO 布局；
- 500 ms 看门狗边界、恢复、断链清零；
- 单腿 FSM 与 ARD-1 策略；
- 安全门条件与授权 epoch；
- PCA9685 脉宽包络、safe-stop、拒绝放宽；
- HUD renderer / 横竖布局；
- 关键词模板匹配（合成特征）；
- MP3 解码（169 帧、44.1 kHz、双声道）；
- 纯 C 源码的 ESP-IDF/FreeRTOS 污染检查。

host suite 不需要 ESP-IDF、开发板或舵机，但它**不能**证明实体动作、供电、限位、显示或声学效果。

### 9.2 HUD 预览

```bash
sh ../tools/hud_preview.sh /tmp/niulai-hud-preview
```

生成横竖两套 PPM 预览图，使用与固件相同的纯 framebuffer renderer。

### 9.3 隔离舵机测试固件

`tools/servo-zero-test/` 是独立的 ESP-IDF 工程，用于 PCA9685 八通道标定、脉宽测量和台架实验。它不接入正式 BLE/FSM/执行器路径。详见 [`tools/servo-zero-test/README.md`](tools/servo-zero-test/README.md)。

---

## 10. 安全清单

1. 先拆下舵机臂，核对 CH0–CH7 的 GND/V+/PWM 方向。
2. 机械固定所有舵机和连杆，人员/线缆/工具退出运动范围。
3. 主电源串入规定的 10 A ATO/ATC 保险；使用受控 6 V 电源并设置限流。
4. 确认可立即断开 XT30；`/OE` 不是断电开关。
5. 第一次上电不装连杆，确认 UART 启动、`/OE` 高、PCA9685 ACK。
6. 从单通道、低幅度、短时开始，逐项记录方向、限位、电流、压降和温升。
7. 出现异响、撞限位、过流、掉压、发热或异常动作，立即 `stop` 并断开 6 V。
8. 软件构建通过 ≠ 实体验收通过；把两者分开记录。

---

## 11. 已知边界

- BLE 台架 profile 使用 LE Secure Connections、无 MITM、无 PIN；适合受控台架，不是生产认证方案。
- PCA9685 的 `/OE` 是动作禁止，不是电源隔离。
- 四路 KW10 的机械触地方向和去抖时长仍需实测。
- MPU6500 的摔倒阈值、滤波和静态基线仍需实体倾斜试验。
- ST7789 的旋转/偏移/颜色和 MAX98357A 的响度/失真仍需实屏/听测。
- 多舵机同时动作的 I²C NACK 与 6 V 瞬态仍需示波器/电流计 A/B；软件重试不能替代电源修复。
- 无编码器：`foot_x/foot_z` 是命令估计，不是真实足端测量。

---

## 12. 延伸阅读

- [BLE 协议](../docs/BLE_PROTOCOL.md)
- [软件架构](../docs/architecture/ESP32_SOFTWARE_ARCHITECTURE_v1.md)
- [控制规格](../docs/specs/CONTROL_SPEC_v0.md)
- [硬件参数登记表](../hardware/electronics/HARDWARE_PARAMETER_REGISTER_v0.1.md)
- [实验索引](../experiments/README.md)
- [host 测试说明](tests/host/README.md)
- [隔离舵机测试固件](tools/servo-zero-test/README.md)
- [MP3 解码器声明](components/mp3_decoder/README.md)
