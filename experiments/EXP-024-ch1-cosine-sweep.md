# EXP-024 CH1 1000↔2000us 余弦连续性测试

## Status

Testing：已完成 host 数学验证、ESP32 固件构建和两次有效的约 10 s 实体运行；机械端
观察尚未完成。

## Objective

验证单独驱动 CH1 时，脉宽能否在 `1000..2000 us` 之间持续往返，并且在 20 ms
控制节拍下保持连续、无明显停顿或跳变。

## Hypothesis

ESP32 内部以 20 ms 固定节拍计算余弦半波，比主机逐帧发送更能保持稳定的时间基准。
每半程使用 `2000 ms`，因此从 1000 到 2000 us 的最大相邻目标变化约为 17 us；余弦
曲线在两个端点的目标速度为零。

## Hardware version

当前隔离 `servo-zero-test` 固件、PCA9685 50 Hz、CH1（RF knee）；其他 CH0、CH2--CH7
保持动作开始时的脉宽，CH8--CH15 为 FULL_OFF。实体动作前由 Operator 确认机械固定、
运动限位、6 V 限流、人员净空和急停。

## Software commit

未提交 exploratory work；工作区分支为 `fw/kw10-perfect-touchdown`。

## Parameters

* CH1 下限：`1000 us`；上限：`2000 us`；不使用角度换算；
* 启动时从当前 CH1 脉宽连续过渡到 `1000 us`；当前值必须已在 `1000..2000 us`；
* 半程：`2000 ms`；ESP32 更新周期：`20 ms`；PCA9685 PWM：`50 Hz`；
* 轨迹：`current → 1000 → 2000 → 1000 → ...`，持续到 `stop`；
* 端点是连续性实验范围，不是机械限位或最终角度标定。

## Test method

1. 确认串口启动日志为隔离 `niulai_servo_zero_test`、PCA9685 `0x40 ACK`、`/OE HIGH`；
2. 确认当前 CH1 脉宽处于 `1000..2000 us`，并发送 `stats reset`；
3. 发送 `demo ch1-cosine`，观察至少一个完整的 `1000→2000→1000` 周期；
4. 记录 CH1 实际方向、端点、是否有卡顿/异响/碰限位，以及 I²C `stats`；
5. 立即发送 `stop`，确认 `/OE HIGH`，再记录异常和电流/温升；
6. 若发现异常，不重复扩大动作，先保留现场日志和视频。

## Video / logs

本轮记录见 [`EXP-024-2026-09-13-ch1-cosine-trial.txt`](logs/EXP-024-2026-09-13-ch1-cosine-trial.txt)。
未制作视频，也没有舵机轴角度、电流、供电压降或温度仪器记录。

## Result

Host 测试已证明：轨迹端点严格为 `1000/2000 us`，全程保持在范围内，20 ms 相邻目标
变化不超过 `17 us`。ESP-IDF v5.5.5 构建通过，最终生成镜像 `0xd1eb0`。首次实体
运行暴露出 CH1 任务栈为 `3072` bytes 时的栈溢出并复位；增至 `4096` bytes 后重新构建、
烧录并完成有效运行。有效运行持续约 `10 s`，启动与停止标记均收到，未再复位或栈溢出；
I²C 统计为 `logical=493 attempts=495 success=493 nack=2 timeout=0 other=0 retries=2
recovered_after_retry=2 terminal_failures=0`，两次首尝试 NACK 均重试恢复。上述结果证明
控制任务和 PCA9685 传输在该次运行中保持工作，不等同于舵机轴的机械连续性验收；实体方向、
真实连续性、端点、限位、电流、噪声和温升仍待 Operator 观察。随后重复演示一次，约
`10 s` 运行无复位、栈溢出或 I²C 错误，统计为 `logical=488 attempts=488 success=488
nack=0 timeout=0 other=0 retries=0 recovered_after_retry=0 terminal_failures=0`，并再次以
`/OE HIGH` 收尾。

## Next step

请 Operator 记录实际方向、是否有卡顿/异响、两个端点附近是否碰限位、其他腿是否保持、
电流/压降和温升。通过后保留 CH1 的纯 us 连续性测试作为单舵机基线；实体失败则先缩小
范围或降低速度，并把结果写回 profile/实验记录，不将本次端点直接冻结为机械安全范围。

## Risk and rollback

这是扩大 CH1 脉宽运动范围的 `HIGH RISK` 实验。运行期间只接受 `stop`、`list`、`help`；
任何异常先发送 `stop`，随后切断 6 V。回滚方式是恢复烧录前的隔离镜像；本命令不接入
正式 BLE/FSM，也不改变 `/OE` 的硬件急停边界。
