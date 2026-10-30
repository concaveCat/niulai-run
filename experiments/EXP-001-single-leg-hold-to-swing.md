# EXP-001 Single-leg Hold-to-Swing

## Objective

验证一条腿能否稳定执行 `press → lift → hold → swing → release → search → contact → support`，并评估这个输入语义是否直观。

## Hypothesis

按下、保持、松开能够让玩家直接表达单腿时序；保持时间可以自然映射到步幅，同时控制器只负责单腿寻地和保护。

## Hardware version

第一阶段：纯软件 mock。实体阶段填写单腿机构、执行器、足端接触输入和固定方式版本。

## Software commit

运行前填写 commit；当前 mock prototype 尚无实体实验 commit。

## Parameters

至少记录 `lift_duration_s`、抬腿轨迹/高度、`swing_speed`、`search_speed`、`max_swing`、`state_timeout_s`、地面和供电条件。

## Test method

1. 在安全限位下按住输入，观察 SUPPORT → LIFT → SWING。
2. 使用不同 hold 时长，比较摆动进度/步幅和动作速度。
3. 松开输入，观察 SEARCH；在不同落脚条件下验证 contact 恢复 SUPPORT。
4. 重复正常、长 hold、无 contact 和超时场景，记录状态日志。

## Questions

- 操作是否直观？
- 保持时间能否自然映射到步幅？
- 动作速度是否有街机感？
- 自动寻地是否可靠？
- 是否需要额外传感器、IMU 或反馈设备？

## Video / logs

待填写。

## Result

待实验。

## Next step

根据结果更新 `007-hold-to-swing-semantics.md`，以及必要的控制规格版本。

