# Open Decisions Register

这里列出当前明确不冻结的事项。只有经过 Experiment / Result 后，才把具体结论写入对应 Decision Record；“未列出详细记录”不代表默认采用或排除。

## Hardware and mechanics

- Leg structure: 2DOF、3DOF 或其他。
- Servo architecture: 类型、型号和总线。
- Controller platform: ESP32 具体型号和平台细节。
- Foot contact: 足端传感器类型和安装方式。
- IMU: 是否需要，以及是否进入闭环。
- Current sensing: 是否需要电流检测。
- PCB: 当前请求方已选择 2 层 Modular Carrier v0.2 作为 Rev A 工作基线；单 XT30、10 A ATO/ATC 主保险、可选 servo-bulk 和通用未用 GPIO breakout 已冻结，Review Candidate 4 原理图和 2026-09-02 Gerber 已归档并在生产后接受 `PASS`，但 exact footprints、实物 pinout、装配和台架验证仍未完成，见 [`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](../../hardware/electronics/MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md) 及对应 review 记录。
- Robot form: 机器人外形与结构细节。

## Product and player experience

- Display HUD v1 已在 [`005-display-requirement.md`](005-display-requirement.md) 接受；最终字体、动效和赛后评分内容仍开放。
- Display placement: 显示屏放在机器人本体还是控制台。
- Control panel: 按钮、脚踏或其他输入形式。
- Feedback: 是否加入 LED、蜂鸣器、实体铃等。
- Scoring/checkpoint: 现有 Android 已进入基于 STATUS 有效步的评分候选 Testing，
  权重、时序窗口和最终 checkpoint/赛道目标仍待实体数据校准；见
  [`020-android-valid-step-scoring.md`](020-android-valid-step-scoring.md)。
- Track: 最终赛道形式。
- Mode: 单人 / 双人哪个是最终主模式。
- Input semantics: Hold-to-Swing 是否保留（当前假设见 [`007-hold-to-swing-semantics.md`](007-hold-to-swing-semantics.md)）。

## Software behavior

- Final leg FSM and final software module boundaries。
- 麦克风语音玩法：Operator 已于 2026-09-13 重新开放 [`012-voice-keyword-command.md`](012-voice-keyword-command.md) 进入 Testing；MS4030 数字采集已通过一次重测，但 `牛来` 的识别率、负样本、触发后的具体玩法及其安全边界仍待定义和验收。
- BLE v1 transport and Android/ESP32 roles are accepted for bring-up in [`009-android-ble-controller-v1.md`](009-android-ble-controller-v1.md); the ESP-IDF project baseline is now covered by [`010-esp-idf-project-baseline.md`](010-esp-idf-project-baseline.md)（Testing，硬件验收后 Accepted）。bonding/authentication and background reconnect policy remain open before physical actuation.
- Embedded audio playback is in software Testing: the current candidate embeds `牛来叫妈妈.mp3` and accepts UART `audio play`/`audio stop` plus the `NIULAI` alias; MAX98357A electrical/audio acceptance and decoder-license review remain open. See [`013-embedded-mp3-command-playback.md`](013-embedded-mp3-command-playback.md).
- 是否需要完整 IK、姿态闭环或更高层协调；这些都不能从 v0 prototype 自动推导为最终需求。

已有详细的第一批 Proposed records：腿部 DOF、舵机、足端接触、IMU、显示、控制台输入和 Hold-to-Swing 语义。其余事项先在此登记，等出现最低成本的验证问题后再单独建 record。
