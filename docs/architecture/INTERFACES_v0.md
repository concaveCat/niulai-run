# INTERFACES_v0 — 最小控制边界

这是为并行探索服务的窄接口，不是最终软件架构。核心 FSM 只处理抽象输入、接触和运动意图。

当前实际需要的能力：

```text
LegActuator.command_leg(command)
FootContactSensor.read_contact() -> bool
PlayerInput.is_pressed() -> bool
```

`BodyAttitudeSensor`、`FeedbackDevice` 等属于可选能力，暂不进入核心 FSM。未来加入显示、LED、蜂鸣器、得分、IMU、电流检测或 checkpoint 时，应优先新增 adapter/capability，而不是让 Leg FSM 依赖具体器件。

实现约束：

- mock 实现用于上海的无硬件开发和测试。
- 硬件实现通过通用 adapter 注入，不在 control 层写死具体舵机、传感器、ESP32 或显示屏。
- command 是运动意图；具体角度、总线帧、采样方式属于 platform/drivers。
- 当前不提供 gait planner 或自动选腿接口。

