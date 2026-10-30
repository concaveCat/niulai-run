# Decision: IMU Requirement

Status:
- Proposed

Problem:
需要判断早期 Hold-to-Swing 实验是否必须感知机身姿态。

Options:
不加入 IMU、加入 IMU 但只记录数据、加入 IMU 并用于保护或控制。

Current hypothesis:
先不把 IMU 放进核心单腿闭环，但可以作为可选记录能力评估其对安全和可玩性的价值。

Experiment:
对比无 IMU 与记录 IMU 数据时的倾倒风险、调试效率和玩家体验。

Result:
待实验。

Decision:
暂不冻结。

Consequences:
若采用，只新增可选 BodyAttitudeSensor/capability，不重写 Leg FSM。

