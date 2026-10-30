# 工具与诊断脚本

这里放无硬件/台架/文档检查工具。原则：

- 优先使用 Python 标准库；
- 需要串口的工具显式依赖 `pyserial`；
- 默认不驱动硬件，只有明确参数才允许输出；
- 实体动作必须有人观察，并能立即断开 6 V / XT30。

```bash
python3 -m pip install -r ../requirements-dev.txt
```

---

## 1. 一键检查

```bash
sh tools/run_offline_checks.sh
```

运行：

- Python 参考控制层测试；
- 工具单元测试；
- 赛后评价代理测试；
- Carrier 纸面检查；
- ESP32 纯 C host suite；
- 隔离舵机 us-profile / 四腿数学测试；
- Markdown 链接、实验索引、Decision 索引；
- 空白检查。

设置 `NIULAI_RUN_ANDROID=1` 可追加 Android `testDebugUnitTest`（需要 JDK/Android SDK）。

单独运行文档审计：

```bash
python3 tools/check_markdown_links.py
python3 tools/check_experiment_index.py
python3 tools/check_decision_index.py
```

---

## 2. BLE 诊断

| 工具 | 用途 |
| --- | --- |
| `ble_diag_probe.py` | 扫描/连接/服务发现/STATUS/IMU 基础探测 |
| `ble_diag_motion_test.py` | 受控发送四腿掩码，观察 STATUS/动作接纳 |
| `ble_home_diag.py` | 验证 HOME 写入、`HOME_COMPLETE` 和断开路径 |
| `reset_windows_ble_bond.py` | Windows 上清理牛来快跑 BLE bond，解决旧配对缓存 |

> 这些工具可能让实体舵机动作。首次使用前必须确认机器人已固定、限位/供电/急停条件满足；默认先只读探测。

---

## 3. 隔离舵机固件工具

配合 [`../firmware-esp32/tools/servo-zero-test/`](../firmware-esp32/tools/servo-zero-test/) 使用：

| 工具 | 用途 |
| --- | --- |
| `servo_zero_console.py` | ASCII 仿 GUI，逐通道调脉宽、保存零位 |
| `servo_zero_tcp_console.py` | 通过 SoftAP/TCP 接收日志并发送 ASCII 命令 |
| `servo_zero_serial_probe.py` | 串口握手/固件信息探测 |
| `servo_zero_serial_benchmark.py` | 有界重复动作统计（I²C NACK/重试） |
| `servo_zero_ch1_cosine_trial.py` | CH1 余弦连续性实验 |
| `servo_zero_quad_trial.py` | 四腿足端空间 demo |
| `servo_zero_rf_hip_vertical_trial.py` | 右前髋后摆、小腿竖直补偿实验 |
| `servo_zero_rf_lift_cosine_drop_trial.py` | 右前腿抬升—余弦前摆—下放实验 |
| `servo_uart_protocol.py` | CRC16 帧、ACK/NACK、`set-target`/`stop` |
| `servo_trace.py` | 串口→PCA9685 分层时序 trace |
| `quad_ascii_walk.py` | 键盘四腿足端空间交互 |

示例（仅示意，不是通用安全参数）：

```bash
python3 tools/servo_zero_console.py --port /dev/ttyACM0
python3 tools/servo_zero_tcp_console.py --host 192.168.4.1
```

这些工具中的 `300..3000 µs`、`500..2500 µs` 等是**软件传输/测量范围**，不是舵机或连杆的机械安全范围。接近机械止挡时必须单通道、低速、小步长，并准备立即断电。

---

## 4. 显示与键盘工具

| 工具 | 用途 |
| --- | --- |
| `hud_preview.sh` | 用与固件相同的纯 renderer 生成横竖 HUD PPM 预览 |
| `quad_ascii_walk.py` | 终端键盘四腿 demo（需要隔离固件） |

```bash
sh tools/hud_preview.sh /tmp/niulai-hud-preview
```

不需要 ESP-IDF、面板或开发板。

---

## 5. 语音/音频工具

| 工具 | 用途 |
| --- | --- |
| `voice_keyword_capture.py` | 通过 UART 采集 `QWVCAP1` 原始 PCM，分析噪声/电平/slot |
| `servo_zero_serial_benchmark.py` | 隔离固件的重复动作与 I²C 统计 |

语音工具只用于 bring-up 诊断，不是通用 ASR，也不接入玩法。

---

## 6. 测试

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py' -v
```

测试不需要真实串口；需要 `pyserial` 的模块在导入时做可选处理。实体串口工具仍需在真实台架上验证。

---

## 7. 安全提醒

- `run` / `press` / `pose` / `demo` 等命令可能立即让舵机动作。
- `/OE` 是动作禁止，不是断电；必须能直接断开 6 V / XT30。
- TCP 诊断接口无认证，只允许本地台架使用，不能暴露到其他网络。
- 任何“串口无 NACK”都不等于机械/供电/温升验收。
