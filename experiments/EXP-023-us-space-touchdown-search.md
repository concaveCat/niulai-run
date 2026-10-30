# EXP-023 纯 us 姿态标尺与触地搜索

## Status

Testing：纯 C host/嵌入式可复用数学模块，尚未接入 CLI、正式执行器或实体动作。

## Objective

为单腿的 support → lift → swing → search → contact → load 流程建立不依赖角度换算的
脉宽标尺。触地姿态由实际 contact 事件确定，不预设“小腿必须垂直于地面”。

## Hypothesis

对每条腿分别记录 support/lift/swing 的 hip/knee us、每个关节的安全范围，以及 knee
朝向地面的脉宽方向、搜索步长和安全上限；在释放后冻结 hip，以小步长单调搜索 knee。
如果 contact 输入已经去抖，锁存的 knee us 就是当前姿态下可重复的触地点；承重目标只
在该点上施加一个可调且有界的 us 偏移。

没有 contact 输入时，单靠 us 数值不能知道地面在哪里；搜索到上限必须报告 exhausted，
不能把上限伪装成成功触地。

## Hardware version

Mock；无实体舵机、PCA9685、KW10 或电源动作。

## Software commit

未提交 exploratory work。

## Parameters

第一版数据结构包含：

* `support`、`lift`、`swing` 三个 `(hip_us, knee_us)` 姿态点；
* hip/knee 各自的 `min_us..max_us`；
* knee `search_direction`、`search_step_us`、`search_limit_us`、`max_search_steps`；
* 触地后施加的有符号 `load_offset_us`，并钳制在 knee 安全范围内。

插值使用整数 Q16 最小跃度曲线；测试采样周期为 20 ms。示例 host profile 仅用于
数学测试，不是 MG90S 标定值：搜索从 `1200 us` 以 `+10 us` 递进，安全上限为
`1500 us`，承重偏移为 `+25 us`。

## Test method

在 `firmware-esp32/tools/servo-zero-test/` 下执行：

```sh
cc -std=c11 -Wall -Wextra -Werror \
  main/servo_zero_us_profile.c tests/test_servo_zero_us_profile.c \
  -o /tmp/niulai-servo-zero-us-profile-test && \
  /tmp/niulai-servo-zero-us-profile-test
```

测试覆盖：

1. 正向/反向最小跃度插值的端点、单调性和 20 ms 相邻脉宽变化上界；
2. 姿态表越界、方向不合法、步长为零、寻地上限方向错误等拒绝路径；
3. hip 在 knee 搜索期间保持冻结，knee 只按配置方向递进；
4. contact 锁存当前 us，load us 由偏移得到并被安全范围钳制；
5. 未触地到达上限、达到最大步数时返回 exhausted，绝不越过上限。

## Video / logs

无；本轮只有 host stdout/exit code。

## Result

2026-09-13：上述命令通过，exit code 为 `0`。整数最小跃度插值不依赖角度或浮点库；
触地搜索只产生安全范围内的脉宽目标，并能区分 `CONTACT` 与 `EXHAUSTED`。

这是软件行为验证，不证明任何 us 值对应特定几何角度、地面高度、接触压力或舵机安全
限位；示例 profile 也没有进入固件命令路径。

## Next step

由 Operator 为一条固定好的实体腿逐项填写 support/lift/swing、hip/knee 安全范围和
寻地方向；先以舵机断电的 mock/输入回放验证 contact 时序，再在机械固定、限流、人员
净空和急停条件齐全后做低幅实体单腿测试。首轮实体测试应记录接触 us、重复误差、
load_offset_us 对稳定性的影响，以及未触地时的 exhausted 行为。
