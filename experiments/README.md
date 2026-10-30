# 实验记录（Experiment Log）

每个编号对应一次可复现的探索或台架实验。目标不是“证明成功”，而是留下足够的事实，让后来的人能判断结论是否仍然成立。

## 约定

- 复制 [`TEMPLATE.md`](TEMPLATE.md) 新建记录，编号唯一、不回收。
- 记录硬件版本、接线、参数、软件 commit（或明确写“未提交”）、重复次数、原始日志/视频位置。
- **失败结果必须保留**；错误结论要明确更正，不能只追加矛盾文本。
- 关键结论链接回对应的 Decision Record 或 Spec。
- 不要把“串口无报错”写成机械/供电/温升验收通过。

原始日志在 [`logs/`](logs/)。

---

## 单腿行为与轨迹（EXP-001 – 011）

- [`EXP-001`](EXP-001-single-leg-hold-to-swing.md)：单腿 Hold-to-Swing 语义。
- [`EXP-002`](EXP-002-servo-ascii-zero-calibration.md)：PCA9685 八通道 ASCII 调零。
- [`EXP-003`](EXP-003-kw10-gpio39-cli.md)：KW10 GPIO39 极性检查。
- [`EXP-004`](EXP-004-rf-leg-lift-lower.md)：右前腿一次性平滑抬起/放下台架测试。
- [`EXP-005`](EXP-005-rest-standing-pose.md)：八路静息站立姿态一次性预览。
- [`EXP-006`](EXP-006-amplitude-scaled-trajectory.md)：按百分比缩放的单腿/静息轨迹。
- [`EXP-007`](EXP-007-pseudo-synchronous-rest-pose.md)：对角交错伪同步静息姿态。
- [`EXP-008`](EXP-008-two-link-foot-trajectory.md)：45/70 mm 两连杆足端抬腿轨迹。
- [`EXP-009`](EXP-009-four-knee-80deg-swing-demo.md)：四髋中立、四膝名义 ±80° 往返 demo。
- [`EXP-010`](EXP-010-joint-ordered-single-leg-trajectory.md)：CH1 先抬、CH0 前摆、CH0/CH1 联合下脚公式。
- [`EXP-011`](EXP-011-continuous-servo-interpolation.md)：连续足端曲线与舵机底层在线插值。

## I²C、供电与串口诊断（EXP-012 – 022）

- [`EXP-012`](EXP-012-esp32-serial-pca-stall-diagnosis.md)：ESP32-S3 串口到 PCA9685 卡顿分层排查。
- [`EXP-013`](EXP-013-motionleg-press-release.md)：motionleg 按下—保持—释放单腿动作。
- [`EXP-014`](EXP-014-crc-uart-and-i2c-isolation.md)：CRC/ACK 串口传输与共享 I²C 隔离排查。
- [`EXP-015`](EXP-015-400khz-i2c-bounded-retry-no-load.md)：400 kHz I²C 与有界重试的无舵机负载验证。
- [`EXP-016`](EXP-016-1mhz-i2c-boundary.md)：1 MHz I²C 速率边界验证。
- [`EXP-017`](EXP-017-20khz-shared-i2c-with-imu.md)：20 kHz 共享 I²C 与 IMU 接入验证。
- [`EXP-018`](EXP-018-100khz-vs-400khz-active-servo.md)：100 kHz 与 400 kHz 带舵机动作的 I²C transaction 统计对照。
- [`EXP-019`](EXP-019-wifi-400khz-50-rounds.md)：USB 拔除条件下 Wi‑Fi TCP 诊断 400 kHz 50 轮。
- [`EXP-020`](EXP-020-usb-powered-400khz-50-rounds.md)：ESP32 USB 供电对照在 PCA9685 启动 ACK 阶段失败。
- [`EXP-021`](EXP-021-direct-gpio-correct-port-50-rounds.md)：修正端口后的直连 GPIO 400 kHz 50 轮。
- [`EXP-022`](EXP-022-adjusted-wiring-ack-probe.md)：调整接线后的 PCA9685 启动 ACK 探测。
- USB 供电对照工具：[`tools/servo_zero_serial_benchmark.py`](../tools/servo_zero_serial_benchmark.py)

## 四腿 / 足端空间 / ARD-1（EXP-023 – 028、034、040）

- [`EXP-023`](EXP-023-us-space-touchdown-search.md)：纯 µs 姿态标尺与有界寻地搜索数学原型。
- [`EXP-024`](EXP-024-ch1-cosine-sweep.md)：CH1 1000↔2000 µs 余弦连续性测试。
- [`EXP-025`](EXP-025-rf-lift-cosine-cutoff-drop.md)：右前腿抬腿、髋余弦前摆、截断后小腿下放 demo。
- [`EXP-026`](EXP-026-rf-lift-cosine-cutoff-drop-60deg.md)：右前腿 CH0 60° 余弦摆幅扩展测试。
- [`EXP-027`](EXP-027-rf-hip-vertical-shank-sweep.md)：CH0 正端点到负端点后摆时保持小腿向下竖直。
- [`EXP-028`](EXP-028-four-leg-foot-space-keyboard-demo.md)：数字键选择单腿、其余三腿足端后退的四腿足端空间 demo。
- [`EXP-034`](EXP-034-ard1-adaptive-reserve-gait.md)：ARD-1 自适应储程大摆腿步态离线工程与验证。
- [`EXP-040`](EXP-040-ard1-geometry-parity.md)：ARD-1 后腿膝分支与 300/360/120 时序对齐验证。

## 音频、语音、IMU bring-up（EXP-029 – 033、042、043）

- [`EXP-029`](EXP-029-voice-keyword-local-template.md)：ESP32-S3 本地“牛来”关键词录入式模板检测器软件验证。
- [`EXP-030`](EXP-030-embedded-mp3-command-playback.md)：嵌入式“牛来叫妈妈”MP3 命令播放软件验证。
- [`EXP-031`](EXP-031-ms4030-i2s-capture.md)：MS4030 I²S 原始采集连通性检查。
- [`EXP-032`](EXP-032-i2s-gpio-loopback.md)：ESP32 I²S GPIO4→GPIO14 电气回环。
- [`EXP-033`](EXP-033-mpu6500-reachability.md)：ESP32-S3 MPU6500 I²C 可达性检查。
- [`EXP-042`](EXP-042-voice-false-positive-guard.md)：“牛来”负样本误报与 I²S 质量门修复验证。
- [`EXP-043`](EXP-043-mpu6500-fall-failure-path.md)：MPU6500 Roll/Pitch 摔倒失败路径的软件验证与固件构建。

## 电源迁移、烧录与 BLE 运行时（EXP-035、036、038、039、041）

- [`EXP-035`](EXP-035-noload-50-rounds-after-servo-power-relocation.md)：舵机电源迁移后的无负载 50 轮 I²C 基线。
- [`EXP-036`](EXP-036-reflash-boot-check.md)：当前 servo-zero 镜像重烧与启动检查。
- [`EXP-038`](EXP-038-actuation-enabled-boot-fix.md)：`ACTUATION_ENABLED` 映射修复、重烧与安全启动检查。
- [`EXP-039`](EXP-039-ble-control-stack-repair.md)：Windows BLE bond、control 栈与通知隔离修复验证。
- [`EXP-041`](EXP-041-ble-home-acceptance-window-fix.md)：BLE HOME 状态 14 与执行器接受窗口修复验证。

---

## 相关入口

- [Decision Record 索引](../docs/decisions/INDEX.md)
- [控制规格](../docs/specs/CONTROL_SPEC_v0.md)
- [BLE 协议](../docs/BLE_PROTOCOL.md)
- [硬件参数登记表](../hardware/electronics/HARDWARE_PARAMETER_REGISTER_v0.1.md)
- [工具说明](../tools/README.md)
