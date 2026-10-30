# EXP-010 关节有序的单腿抬起、前摆与下脚公式

## Status

Testing：已完成纯数学实现和 host 测试；尚未把角度转换成舵机脉宽，也没有驱动实体执行器。

## Objective

按 Operator 指定的顺序建立右前腿候选轨迹：先进入静息姿势，随后只动 CH1 抬膝，
再只动 CH0 向前旋转，最后根据足端目标用 CH0/CH1 联合 IK 下脚。CH0 的前摆幅度作为
独立参数，便于比较不同幅度的可达性和关节变化。

本实验只修改隔离的纯数学模块，不接入 BLE、FSM、正式执行器 adapter 或实体动作。

## Geometry and coordinate convention

- 大腿 `L1=45 mm`，小腿 `L2=70 mm`。
- 髋为原点；`x` 正方向为前摆，`z` 正方向为抬脚。
- `q_h` 是大腿相对 `+x` 的角度；`q_k` 是相对大腿的膝角；小腿绝对角为
  `q_h+q_k`。

正运动学：

```text
x = L1 cos(q_h) + L2 cos(q_h + q_k)
z = L1 sin(q_h) + L2 sin(q_h + q_k)
```

足端 `p=(x,z)` 的逆运动学：

```text
c_k = (x² + z² - L1² - L2²) / (2 L1 L2)
q_k = s · acos(clamp(c_k, -1, 1))
q_h = atan2(z, x) - atan2(L2 sin(q_k), L1 + L2 cos(q_k))
```

其中 `s=+1/-1` 选择两条膝支路。几何可达条件仍为
`|L1-L2| <= sqrt(x²+z²) <= L1+L2`，即 `25..115 mm`。

## Joint-ordered trajectory

给定静息关节角 `q0=(q_h0,q_k0)`、抬高量 `c` 和 CH0 前摆角 `Δq_h`：

### 1. CH1-only knee lift

先求静息足端 `p0=FK(q0)`，目标高度为 `z_l=p0.z+c`。髋保持不动，只解膝角：

```text
sin(q_h0 + q_k,lift) = (z_l - L1 sin(q_h0)) / L2
```

候选小腿绝对角为 `asin(s)` 和 `π-asin(s)`；选择满足预定膝支路方向且距离
`q_k0` 最近的候选，得到
`q_lift=(q_h0,q_k,lift)`。如果没有候选或目标高度不可达，整个计划无效。

### 2. CH0-only forward swing

膝保持在 `q_k,lift`，髋按设定幅度前摆：

```text
q_swing = (q_h0 + Δq_h, q_k,lift)
p_swing = FK(q_swing)
```

实现会对该弧线采样，要求足端始终不低于静息地面 `z₀`；这避免髋前摆过程中重新
擦地。`Δq_h` 可以逐档设置，例如 `0°、5°、10°、15°、20°`。

### 3. CH0+CH1 联合 touchdown

下脚目标不是独立猜一个 CH0 或 CH1 脉宽，而是定义足端目标：保持前摆后的 `x`，
回到静息地面高度：

```text
p_touch = (p_swing.x, p0.z)
```

下脚期间用五次平滑步进 `S(u)=10u³-15u⁴+6u⁵` 生成足端路径：

```text
p(u) = (p_swing.x,
        p_swing.z + (p0.z - p_swing.z) S(u)),  0 <= u <= 1
```

对每个 20 ms 采样点执行 `IK(p(u), s)`，得到同时变化的 `q_h(u),q_k(u)`。这样下脚
会保持足端前后位置并连续落到地面，且不会发生膝支路跳变。若任一采样点超出可达域，
该摆幅直接标记为无效。

阶段时序为：静息保持 `400 ms` → CH1 抬膝 `500 ms` → CH0 前摆 `650 ms` →
CH0/CH1 联合下脚 `1000 ms`。下脚比前摆更慢，用于限制 IK 角度单步变化。

## Nominal model sweep

测试输入：静息足端 `(0,-100) mm`、抬高 `20 mm`、正膝支路、20 ms 采样。

| CH0 model forward swing | Result |
| ---: | --- |
| 0° | valid |
| 5° | valid |
| 10° | valid |
| 15° | valid |
| 20° | valid |
| 25° | invalid: touchdown point exceeds the 115 mm outer reach for this nominal branch |

这不是实体机械限位；它只是 `45/70 mm` 平面模型对该静息点、抬高量和膝支路的几何边界。

## Acceptance criteria

1. FK/IK 闭环误差小于 `0.01 mm`。
2. CH1 抬膝阶段只改变 `q_k`；CH0 前摆阶段只改变 `q_h`。
3. 联合下脚阶段每个 20 ms 足端采样点均可 IK，且 `z` 单调下降到 `z₀`。
4. 全轨迹不低于静息地面；相邻 20 ms 关节角变化小于 `0.05 rad`。
5. 不把模型角度直接当作舵机脉宽；实体前仍需逐通道角度/脉宽、方向、限位、电流和
   足端位置标定。

## Software evidence

- 新增 `servo_zero_leg_single_params_t`、`servo_zero_leg_single_plan_t` 和
  `servo_zero_leg_single_pose()`。
- 新增实验映射 `servo_zero_leg_map_angles_to_pulses()`；当前 RF 临时映射为
  CH0 `+14.6 us/°`、CH1 `-10.9 us/°`，基准静息脉宽为 `1494/1165 us`，只用于验证，
  不冻结为最终标定。
- `servo_zero_leg_make_single_plan()` 负责 CH1 抬高求解、CH0 摆幅可达性检查、
  touchdown 目标构造和整条路径预验证。
- host C 测试覆盖阶段端点、联合下脚、20 ms 可达性/角度步长以及 `0..20°` 有效、
  `25°` 无效的 CH0 摆幅扫描。
- 隔离固件新增 `step rf-ik DEG`，执行同一计划器。

2026-09-12：将含 `step rf-ik DEG` 的隔离固件重新构建，镜像大小为 `0x469e0`，app
分区余量约 72%；bootloader、app、partition table 三段烧录均报告 `Hash of data verified`。
随后执行一次 `step rf-ik 5`。UART 回显静息基准 CH0/1=`1494/1165 us`、模型抬高 `5.0 mm`、
前摆 `5.0°`、预计下脚 CH0/1=`1673/1202 us`、总时长 `2550 ms`；完成回显足端目标
`(17.8,-100.0) mm` 和 `/OE HIGH`，全程无 I²C/NACK。最后 `stop + load + list` 确认八路
恢复直立脉宽；实体抬脚、前摆、下脚平滑性、限位、电流、噪声和温升仍待 Operator 观察。

## Next step

先记录本次 `5°` 动作的实际抬脚/前摆/下脚结果；若方向和供电状态正常，再逐档比较
`10°`、`15°`、`20°`。随后根据实体静息姿态和足端地面高度替换名义 `(0,-100) mm`，
并重新标定角度到脉宽映射；当前命令仍是 HIGH RISK 实验路径，不接入正式控制链路。
