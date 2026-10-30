# Decision: Servo Choice and Bus

Status:
- Proposed

Problem:
需要选择能满足原型负载、速度、反馈和接线约束的执行器方案。

Options:
不同舵机类型、型号、通信总线或本地驱动方式。

Current hypothesis:
尚无偏好。先建立候选清单并在单腿上比较扭矩、响应、回差、可维护性和保护能力。

Experiment:
在安全限位下测量候选执行器的负载、响应和连续工作表现。

Result:
待实验。

Decision:
暂不冻结。

Consequences:
会影响 electronics、platform 和 actuator adapter；控制逻辑只依赖窄的 LegActuator 能力。

