# Decision Record Index

Last reviewed: 2026-09-17

这是 Decision Record 的导航索引，不替代记录正文、[`OPEN_DECISIONS.md`](OPEN_DECISIONS.md)
或人类 review。状态和理由以各文件正文为准；旧硬件设计输入另见
[`hardware/electronics/archive/legacy-integrated-controller/`](../../hardware/electronics/archive/legacy-integrated-controller/)。

仓库保留了两个并行工作流都使用 `010` 数字前缀的历史文件。为避免破坏既有引用，未擅自重命名；完整文件名是唯一索引键。

| Record | Status | Scope |
| --- | --- | --- |
| [`001-leg-dof`](001-leg-dof.md) | Proposed | 单腿机械自由度 |
| [`002-servo-choice`](002-servo-choice.md) | Proposed | 执行器型号与总线 |
| [`003-foot-contact-sensing`](003-foot-contact-sensing.md) | Proposed | 足端触地传感 |
| [`004-imu-requirement`](004-imu-requirement.md) | Proposed | IMU 是否进入记录/闭环 |
| [`005-display-requirement`](005-display-requirement.md) | Accepted for TFT HUD v1 | 显示与现场反馈 |
| [`006-control-panel-input`](006-control-panel-input.md) | Proposed | 控制台输入形式 |
| [`007-hold-to-swing-semantics`](007-hold-to-swing-semantics.md) | Proposed | Hold-to-Swing 语义 |
| [`008-pcb-controller-v0.1`](008-pcb-controller-v0.1.md) | Proposed | Modular Carrier 原型基线 |
| [`009-android-ble-controller-v1`](009-android-ble-controller-v1.md) | Accepted for BLE bring-up | Android BLE Central 与 v1 协议 |
| [`010-android-llm-gameplay-feedback`](010-android-llm-gameplay-feedback.md) | Accepted for Android Test Ctrl prototype | 可选赛后评价服务 |
| [`010-esp-idf-project-baseline`](010-esp-idf-project-baseline.md) | Testing | ESP-IDF + ESP-NimBLE 固件端点 |
| [`012-voice-keyword-command`](012-voice-keyword-command.md) | Testing | ESP32-S3 本地“牛来”关键词唤醒 |
| [`013-embedded-mp3-command-playback`](013-embedded-mp3-command-playback.md) | Testing | 嵌入式 MP3 资源与 UART 命令播放 |
| [`014-gait-architecture-codenames`](014-gait-architecture-codenames.md) | Testing | KFC-1 现行架构归档与 ARD-1 自适应储程后继架构 |
| [`015-esp32-layered-runtime`](015-esp32-layered-runtime.md) | Preliminary Accepted (software-only) | ESP32 选择性 HAL、驱动、服务、运动策略与应用边界 |
| [`016-ble-runtime-interaction`](016-ble-runtime-interaction.md) | Testing | BLE 归位交互与 MPU6500 动态加速度遥测 |
| [`017-no-pin-actuation-bench-profile`](017-no-pin-actuation-bench-profile.md) | Testing / HIGH RISK | 免 PIN 加密绑定与固定台架实体执行器 profile |
| [`018-ble-home-and-acceptance-window`](018-ble-home-and-acceptance-window.md) | Testing / HIGH RISK | 1 秒执行器接受窗口与 HOME admission 竞态修复 |
| [`019-mpu6500-fall-game-failure`](019-mpu6500-fall-game-failure.md) | Testing / HIGH RISK | MPU6500 Roll/Pitch 摔倒失败、安全门和 Android 结算边界 |
| [`020-android-valid-step-scoring`](020-android-valid-step-scoring.md) | Testing | Android 基于 STATUS 有效步的速度分与协调分候选 |

新增重大决定时，先复制 [`TEMPLATE.md`](TEMPLATE.md)，再在本页登记文件名和当前状态；不要用索引行代替正文的 Result、Consequences 或 supersession 记录。
