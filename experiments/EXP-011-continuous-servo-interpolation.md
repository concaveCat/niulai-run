# EXP-011 连续单腿轨迹与舵机底层在线插值

## Status

Testing：纯 C 测试和 ESP-IDF 构建验证中；尚未烧录或进行实体动作质量验收。

## Objective

解决两类叠加的离散化：实验轨迹在“抬膝/前摆/下脚”边界反复把速度降到零，以及正式
PCA9685 adapter 把每个 FSM 目标直接写入舵机。范围限于单腿局部轨迹和执行器 adapter，
不增加 gait planner、自动选腿、补步或全身平衡。

## Observation and hypothesis

- 观测：旧 `step rf-ik` 的三个阶段分别使用五次平滑函数；每段端点速度、加速度均为零，
  因而中间两处必然停顿。
- 观测：正式 `actuator_pca9685` 将 `qw_pose_map()` 产生的脉宽直接写 CHx/CHx+1；例如
  LIFT 终点到 SWING 起点可出现姿态级跳变，且两通道原先是两条 I²C 事务。
- 假设：单段足端曲线消除内部停点，adapter 再对任意在线目标施加速度/加速度限制，能
  同时改善已知轨迹和未来上层目标的连续性。

## Proposed implementation

### Single continuous foot arc

`step rf-ik DEG` 改用一条三次 Bézier 足端曲线。起点 `P0` 为静息足端，终点 `P3` 为前摆
后的地面点；`P1` 与 `P0` 同 x，`P2` 与 `P3` 同 x，两控制点高度均为
`z0 + 4h/3`。因此起落切线竖直、曲线中点恰好达到 `z0+h`，内部没有人为速度归零点。
整条曲线用一个五次时间标度，只在完整动作起点和终点停下。每个 20 ms 样本重新做 IK。

### Driver-level interpolation

正式 adapter 为活动腿两路各保存连续的 `position_us/velocity_us_s`。每个 100 Hz 控制 tick：

1. 用剩余误差计算可制动速度 `sqrt(2*a*|error|)`；
2. 目标速度限制到 `±v_max`；
3. 本 tick 的速度变化限制到 `±a_max*dt`；
4. 积分得到下一脉宽，越过目标时钳位到目标。

候选默认值为 `v_max=900 us/s`、`a_max=3600 us/s²`，均由 Kconfig 配置，不是冻结的舵机
规格。100 Hz 下稳态单 tick 位移上限为 `9 us`。若调度延迟超过两个名义周期，adapter
按一个名义周期推进，防止一次延迟形成大跳变。

PCA9685 MODE1 保持 `AI`，活动腿两路用一条 9-byte auto-increment 事务提交，使同一控制
样本原子更新。`force_safe_stop()` 不经过插值：仍立即 `/OE HIGH`，写中立，再 SLEEP。

## Acceptance criteria

1. 阶跃目标全程不越过目标，速度不超过 `900 us/s`，非最终钳位 tick 的速度变化不超过
   `36 us/s`，10 ms 位移不超过 `9 us`。
2. 在线目标突然反向时仍满足同一速度/加速度界限。
3. 20°、5 mm 连续足端轨迹的 20 ms 样本全部可达、不低于地面，最大相邻关节变化
   `<0.02 rad`，映射后最大相邻脉宽变化 `<=20 us`。
4. 活动腿两路由一条 I²C auto-increment 帧提交；安全停止顺序不变。
5. host 测试、严格 C 数学测试、ESP-IDF 构建通过。
6. 实体“不卡顿”只能由 Operator 在固定、限位、限流、净空和急停齐全的台架确认；
   软件测试不得冒充实体动作验收。

## Result

待本轮验证完成后填写。实体重复次数：0；UART 日志/视频：无。

## Risk and rollback

这是改变速度、加速度和执行器行为的 `HIGH RISK` 改动。回滚方式是恢复 adapter 的直接
目标写入和旧 `servo_zero_leg_single_pose()` 调用；安全 gate、`/OE` 与 SLEEP 路径不变。
MR 必须由非作者人类成员审阅，实体测试必须从低幅度开始。
