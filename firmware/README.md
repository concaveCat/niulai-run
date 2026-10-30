# Python 参考控制层（firmware/）

这是牛来快跑的**硬件无关参考实现**，用于在电脑上验证控制语义、FSM 和 PCA9685 寄存器协议。正式机器人运行的是 [`../firmware-esp32/`](../firmware-esp32/) 中的 C 固件；本目录不是它的运行时依赖，而是：

- 单腿 Hold-to-Swing 行为的可读参考；
- ESP32 C 端口的上游语义来源；
- 无硬件 mock 开发和回归测试；
- PCA9685 参数化驱动的 Python 版本。

```text
firmware/
├── control/
│   ├── interfaces.py      LegActuator / FootContactSensor / PlayerInput
│   ├── leg_fsm.py         SUPPORT → LIFT → SWING → SEARCH → SUPPORT / FAULT
│   ├── controller.py      能力组合与 tick
│   └── mock.py            mock 输入/接触/执行器
├── platform/
│   ├── pca9685.py         PCA9685 寄存器/脉宽数学
│   ├── defaults.py        通用候选默认值（不是实测值）
│   └── callback_adapters.py 回调式 adapter
└── tests/                 unittest
```

---

## 运行测试

```bash
python3 -m unittest discover -s firmware/tests -p 'test_*.py' -v
```

覆盖：

- `SUPPORT → LIFT → SWING → SEARCH → SUPPORT` 正常流程；
- 长按上限、释放、触地、超时进入 `FAULT`；
- 非法输入不崩溃；
- `dt` 边界和状态时间；
- `reset()` 保留历史；
- PCA9685 脉宽/频率/通道数学和参数化标定闸门。

---

## 设计边界

- **FSM 只输出运动意图**：`LegCommand(mode, progress)`，不输出角度、脉宽或 I²C 帧。
- **具体硬件通过 capability 注入**：`LegActuator`、`FootContactSensor`、`PlayerInput` 是 Protocol。
- **缺少标定就拒绝**：`ServoCalibration` 的字段默认是 `None`；不完整时 `pulse_for_angle()` 抛 `ParameterTBDError`，不会猜一个安全值。
- **默认值只是 bring-up 候选**：`defaults.py` 中的 500–2500 µs、50 Hz、0x40 等不是机械安全范围，也不是最终标定。

当前不包含：

- 完整 gait planner；
- 自动选腿/补步；
- 全身平衡；
- 实体总线/GPIO 访问。

ESP32 端的 C 实现见 [`../firmware-esp32/README.md`](../firmware-esp32/README.md)；控制语义见 [`../docs/specs/CONTROL_SPEC_v0.md`](../docs/specs/CONTROL_SPEC_v0.md)。
