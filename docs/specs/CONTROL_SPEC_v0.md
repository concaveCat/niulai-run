# CONTROL_SPEC_v0 — Hold-to-Swing 实验规格

> 这是当前实验规格，不是最终控制协议。它只描述单腿行为，不包含 gait planner、自动选腿或全身平衡。

## 状态

```text
SUPPORT → LIFT → SWING → SEARCH → SUPPORT
```

原型还可以进入 `FAULT` 安全停止状态；它不是玩法状态。

| 状态 | 进入条件 | 当前动作 | 离开条件 |
| --- | --- | --- | --- |
| SUPPORT | 初始或 contact | 保持支撑 | button press → LIFT |
| LIFT | SUPPORT 检测到 press | 抬腿 | 抬腿完成且仍 hold → SWING；已 release → SEARCH |
| SWING | LIFT 完成 | 持续摆动；hold 时长影响摆动进度/步幅 | release → SEARCH |
| SEARCH | release 或 LIFT 中 release | 开始落下/寻地 | foot contact → SUPPORT；超时 → FAULT |
| FAULT | 状态超时或保护条件 | 安全停止并记录原因 | 显式 reset |

## 输入语义

- `button press` 是从未按下到按下的边沿，触发 `SUPPORT → LIFT`。
- `button hold` 维持在 `LIFT`/`SWING` 路径；在 `SWING` 中持续推进摆动，直到达到可调上限。
- `button release` 触发 `SWING → SEARCH`；若在 `LIFT` 中松开，则抬腿完成后进入 `SEARCH`。
- `foot contact` 只在 `SEARCH` 中用于恢复 `SUPPORT`。触地传感器的物理实现未冻结。

## 玩家与控制器的边界

玩家控制：腿动作何时开始、何时释放以及由保持时长形成的当前步幅/时序。

控制器自动完成：单腿轨迹执行、必要的 IK、传感器读取、关节/硬件保护、落下/寻地动作和 contact 后恢复支撑。

当前禁止：自动选择下一条腿、自动生成完整 gait、玩家失误后的自动补步/恢复平衡、完整全身平衡控制。

## 可调实验参数

以下都是配置参数，不是冻结的机械或产品规格：

- `lift_duration_s` / 抬腿高度或轨迹参数
- `swing_speed` / 摆动速度
- `search_speed` / SEARCH 速度
- `max_swing` / 最大步幅或等价摆动上限
- `state_timeout_s` / 状态超时

具体单位和默认值由当前原型配置决定；修改默认值时记录实验结果，不要把它们当作最终调参。

## 非目标

当前版本不定义具体 ESP32 型号、舵机、足端传感器、IMU、显示设备、总线、完整 IK 或最终模块划分。硬件替换应通过 adapter/capability 完成。

