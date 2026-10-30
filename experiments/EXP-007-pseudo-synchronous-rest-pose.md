# EXP-007 对角交错伪同步静息姿态

## Status

Testing：目标是在不重复触发已观察到的八路同步 PCA9685 NACK 的前提下，让四条腿在视觉上
接近同时、连续地从垂直直立位进入静息姿态。该实验仍属于隔离舵机测试，不进入正式
驱动或 gait。

## Objective

验证快速连续发送 UART 指令是否能提供可靠同步，并以 ESP32 内部单条命令的对角交错轨迹
替代主机连发。成功条件是软件无 NACK、轨迹端点正确、实体动作平滑且四条腿方向和限位
均由 Operator 确认。

## Hypothesis

主机快速连发不能提供原子同步，也不能消除多舵机同时启动的电流瞬态。将此前通过四通道
测试的两组对角腿错开少量时间，并由 ESP32 固定 20 ms 节拍推进，可能在视觉上接近同步，
同时降低单帧主动变化的通道数和负载突变。

## Hardware version

- ESP32-S3 N16R8；Carrier Gerber 映射 SDA=GPIO11、SCL=GPIO12、OE=GPIO1。
- PCA9685 地址 `0x40`、PWM 50 Hz；舵机电源为独立受控 6 V，共地。
- 对角组 A：CH0/CH1/CH6/CH7；对角组 B：CH2/CH3/CH4/CH5；CH8--CH15 保持
  `FULL_OFF`。
- 实体动作前须确认机械固定、前后大腿限位、6 V 限流、人员净空和可立即切断 6 V 的急停。

## Software version

2026-09-12，分支 `fw/kw10-perfect-touchdown` 的未提交工作区；代码先在
`firmware-esp32/tools/servo-zero-test/` 隔离验证，只有本实验通过后才考虑正式驱动集成。

## Parameters

- 从已确认直立值 `1746,1469,1427,1415,1619,1582,1449,1600 us` 出发。
- 目标为 Operator 实测静息值 `1494,1165,1672,1748,1368,1279,1764,1934 us`。
- 当前 V2：直立稳定 `400 ms`；对角组错开 `120 ms`；每组最小跃度移动 `2000 ms`；
  总计约 `2520 ms`。V1 的 `1200 ms` 余弦/交替更新结果保留在下方历史记录中。
- V2 每 `20 ms` 同时计算两组并提交一条八路原子整帧；任一 I²C 错误立即停止并将 `/OE`
  拉高。
- `pose pseudo [PCT]` 的 `PCT` 为 `0..100`；建议按 `25 → 50 → 100` 逐档执行。

## Acceptance criteria

1. 主机只发送一条 `pose pseudo PCT`，不依赖多条 UART 指令的时序。
2. 纯 C 数学测试和 ESP-IDF 构建通过；烧录三段镜像均 Hash 校验通过。
3. 每个幅度至少完成一次有界动作，无 PCA9685 NACK；异常时 `/OE HIGH` 且 `stop` 可收尾。
4. Operator 观察到四腿启动时间接近、动作无明显阶跃或抖动，方向正确且没有撞限位、
   拖地、异常噪声、过流或快速温升。
5. 在达到上述证据前，不把命令复制到正式执行器驱动，也不冻结为 gait 行为。

## Test method

1. 运行 host 数学测试、`git diff --check` 和 ESP-IDF build；依据 `flasher_args.json`
   检查端口、烧录和启动日志。
2. 确认实体安全条件后，先发送 `pose pseudo 25`，等待完成后发送 `stop`；记录 UART、
   方向、平滑性、电流/压降、限位、噪声和温升。
3. 无异常才执行 `pose pseudo 50` 和 `pose pseudo 100`；每次结束发送 `stop`，发现
   NACK 或机械异常立即切断 6 V。
4. 将失败结果保留；串口 `reached` 只证明轨迹任务完成，不替代实体观察。

## Result

数学测试和固件构建已通过；EXP-007 镜像 `0x43c40` 已按 `flasher_args.json` 烧录到 CH340。
`pose pseudo 25` 和随后按 Operator 要求执行的 `pose pseudo 100` 均在约 `1720 ms` 到达
目标，UART 未出现 NACK，且每次均以 `stop` 收尾并拉高 `/OE`。Operator 反馈 25% 幅度现场
完全看不出明显幅度；随后 Operator 确认 100% 动作成功。

为排除静摩擦，曾用快速连续 `select/pulse` 命令同步推进八路；UART 出现 `unknown command`
和错误通道脉宽，说明该测试被串口解析失步污染，不能作为实体证据。新增单条
`frame US0,...,US7` 后，八路同步 5 μs 微步连续 10 轮（累计 50 μs）均按预期回显，未出现
命令错位或 NACK，最终 `stop + load` 安全收尾。该结果证明原子整帧命令路径稳定，但不单独
证明每个舵机都克服了死区或静摩擦。

### V2 smoothing revision

针对 Operator 观察到的明显卡顿，V2 将伪同步曲线改为最小跃度多项式
`10t^3 - 15t^4 + 6t^5`，使动作端点的速度和加速度都为零；每组移动时间由 `1200 ms`
增至 `2000 ms`，并取消每个舵机 40 ms 才更新一次的交替写法。ESP32 仍只接受一条启动
命令，轨迹在本机 20 ms 节拍运行，异常时不重试 I²C 而直接关闭 `/OE`。V2 代码尚待构建、
烧录及 Operator 实体平滑性/NACK 复测，不能把该设计变更当作已验收。

V2 验证结果：host 数学测试和 `git diff --check` 通过；ESP-IDF v5.5.5 构建生成应用
`0x43ef0`，并按 `build-servo-zero/flasher_args.json` 经 CH340 烧录，bootloader、
partition table 和 app 均报告 `Hash of data verified`。在 2026-09-12 的有界实测中，
`pose pseudo 25` 和 `pose pseudo 100` 均到达 `rest pose reached`（约 2520 ms），UART
未出现 NACK 或 ESP32 重启；每次均自动执行 `stop + load`，最终 `/OE HIGH`。本结果只证明
软件轨迹、串口启动和 I²C 事务完成，Operator 对实体动作的连续性、方向、限位、电流/压降、
噪声和温升仍需单独确认。

## Next step

先完成 V2 host 数学测试、ESP-IDF 构建和 CH340 有界烧录，再执行 `pose pseudo 25`；若
无 NACK、过流、撞限位或方向错误，再逐步执行 50% 和 100%。Operator 需回报平滑性、方向、
限位、电流/压降、噪声和温升。仅当软件和实体观察均满足 acceptance criteria 后，创建正式
驱动集成任务。
