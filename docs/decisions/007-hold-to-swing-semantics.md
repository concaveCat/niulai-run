# Decision: Hold-to-Swing Input Semantics

Status:
- Proposed

Problem:
需要验证按下、保持、松开是否形成清晰、有街机感且可学习的单腿时序。

Options:
Hold-to-Swing、其他按键时序或更连续的输入映射。

Current hypothesis:
Hold-to-Swing 是第一项实验假设：press → lift，hold → swing，release → search，contact → support；不代表最终玩法。

Experiment:
执行 [`EXP-001`](../../experiments/EXP-001-single-leg-hold-to-swing.md)，从玩家反馈和运动数据评估。

Result:
待实验。

Decision:
暂不冻结。

Consequences:
当前只影响 CONTROL_SPEC_v0 和 mock FSM；若被推翻，新增 CONTROL_SPEC_v1 或新的 Decision Record。

