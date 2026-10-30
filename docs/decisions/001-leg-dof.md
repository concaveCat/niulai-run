# Decision: Leg DOF

Status:
- Proposed

Problem:
需要确定单腿的机械自由度，以便估算动作能力、成本和控制难度。

Options:
2DOF、3DOF 或其他结构。

Current hypothesis:
尚无偏好。先用最低成本的单腿原型验证玩家是否能感知并控制腿间时序，再比较可达工作空间和结构复杂度。

Experiment:
用可快速改造的单腿/简化机构测试抬腿、摆动、寻地和支撑；记录所需工作空间、负载和可控性。

Result:
待实验。

Decision:
暂不冻结。

Consequences:
会影响机械结构、舵机数量、IK 边界、供电和固件 actuator adapter；核心 FSM 不应依赖具体 DOF。

