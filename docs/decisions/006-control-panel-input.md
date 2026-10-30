# Decision: Control Panel Input Form

Status:
- Proposed

Problem:
需要选择最能让玩家表达腿时序的实体输入形式。

Options:
按钮、脚踏或其他输入机构；单人/双人模式也未冻结。

Current hypothesis:
Hold-to-Swing 先用可替换的 PlayerInput 抽象和简单 mock 验证语义，再用实体输入比较可操作性和疲劳。

Experiment:
比较不同输入形式的时序准确性、直观性、并发操作和可扩展性。

Result:
待实验。

Decision:
暂不冻结。

Consequences:
只影响输入 adapter、控制台硬件和绑定配置，不应进入 Leg FSM 的硬件细节。

