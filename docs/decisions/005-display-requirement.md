# Decision: Display and Feedback Requirement

Status:
- Accepted for the TFT HUD v1 by the requesting Operator on 2026-09-10

Problem:
需要判断显示屏、LED、蜂鸣器或实体铃是否能改善玩家反馈和比赛可读性。

Options:
无额外反馈、LED/蜂鸣器、显示屏或其他 FeedbackDevice 组合；显示屏可以仅用于调试，也可以承担玩家可见的现场状态和游戏反馈。安装位置仍未确定。

Current hypothesis:
已装配的 170×320 ST7789 可以承担低延迟、无需手机即可看到的现场状态和游戏反馈；复杂设置仍留在 Android 控制器。

Experiment:
先用隔离于 BLE、FSM 和执行器的彩条/HUD demo 验证面板、电气链路、固定布局和颜色语义；后续再把真实状态通过只读 adapter 接入。

Result:
2026-09-10 Operator 确认 ST7789 已实际点亮。随后接受首版信息架构，并要求开始生成：顶部固定状态栏、中部四腿状态、底部安全状态。HUD demo 已通过 host layout/model tests 和 ESP-IDF native build，并成功烧录；实际 UI 方向、文字可读性和颜色仍需视觉验收。
2026-09-16 Operator 要求底部栏不再使用纯演示内容。正常 runtime HUD 现通过只读 MPU6500 telemetry snapshot 显示去除静态重力基线后的动态加速度代理值；`DISPLAY_HUD_TEST` 仍保留为隔离的显示预览，不代表真实传感器数据。

Decision:
当前阶段采用 TFT HUD。v1 固定包含：

- 顶部 28 px 状态栏：BLE、开机持续运行时间（UP）、总体安全状态；本机不宣称检测电池电量；
- 中部：FL/FR/RL/RR 四腿的玩家按下和触地状态，以及主要阶段反馈；
- 中部：真实 `SAFE/WARN/FAILSAFE/FAULT` 状态；
- 底部 36 px 加速度栏：`ACC x.xxG`、加速度等级颜色和 `/OE` 状态；
- 蓝色表示连接/输入，绿色表示正常/触地，黄色表示等待/警告，红色表示 failsafe/故障，灰色表示未启用或无数据。
- 加速度栏使用去除静态重力基线后的动态加速度代理值：低于 `0.25 g` 为绿色，`0.25–<0.60 g` 为黄色，`≥0.60 g` 为红色；未完成静态标定、无效、削顶或超过 500 ms 未更新为灰色 `ACC --`。这些是显示阈值，不是机械安全阈值。该标量代理值不是姿态解算，不进入平衡闭环。

显示层只消费状态，不改变 FSM、不选择腿、不补步、不直接控制执行器。屏幕安装位置、电池测量来源、最终字体/动效和赛后评分内容不在本 Decision 中冻结。

Consequences:
显示成为当前原型的用户可见能力，但继续作为与核心运动控制低耦合的输出 capability。bring-up 模式必须保持 PCA9685 `/OE` HIGH；真实运行接入应通过只读快照，显示失败不得改变安全门或运动状态。
