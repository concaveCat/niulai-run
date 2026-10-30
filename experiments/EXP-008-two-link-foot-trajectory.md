# EXP-008 45/70 mm 两连杆足端抬腿轨迹

## Status

Testing：已完成纯 C 的正/逆运动学和轨迹可达性测试；尚未进行实体舵机动作，不能视为
最终腿部轨迹或舵机角度标定。

## Objective

使用 Operator 提供的几何尺寸建立隔离的二维单腿模型：大腿长度 `45 mm`（髋舵机轴到
膝舵机轴），小腿长度 `70 mm`（膝轴到脚底参考点）。在足端空间设计可按抬高和前摆幅度
缩放的抬腿/迈步候选轨迹，并用 IK 生成关节角。

该实验只扩展 `servo-zero-test` 的纯数学模块，不接入正式 FSM、BLE、执行器 adapter、
自动选腿或全身平衡。

## Coordinate convention

- 髋关节为原点；`x` 正方向为腿的前摆方向；`z` 正方向为抬脚方向。
- 髋角 `q_h` 从 `+x` 轴测量；膝角 `q_k` 是相对大腿角，小腿绝对角为 `q_h + q_k`。
- 正运动学：

  ```text
  x = 45 cos(q_h) + 70 cos(q_h + q_k)
  z = 45 sin(q_h) + 70 sin(q_h + q_k)
  ```

- 可达半径为 `25 mm <= sqrt(x² + z²) <= 115 mm`。每个轨迹采样点都必须通过 IK，
  但这只是几何可达性，不代表实体机构不会干涉或撞止挡。

## Candidate trajectory

名义起点和幅度为实验假设，不是实测姿态：

| Phase | Time | Foot path |
| --- | ---: | --- |
| clearance lift | 500 ms | `(0, -100) -> (0, -100 + lift)` |
| forward swing | 650 ms | 保持抬高，`x: 0 -> forward` |
| touchdown | 800 ms | 保持前摆位置，`z: -100 + lift -> -100` |

其中完整幅度使用 `lift=20 mm`、`forward=30 mm`，总时长 `1950 ms`；各阶段使用五次
平滑步进，阶段端点速度和加速度为零。幅度 `0..100%` 同时缩放 `lift` 和 `forward`，
不会直接缩放舵机脉宽。

这条路径的意图是：先让脚底获得明确离地余量，再在高位前摆，最后以比前摆更慢的速度
下降。实际的膝支路（forward/backward branch）必须在装配后由 Operator 观察确认。

## Acceptance criteria

1. 正/逆运动学闭环误差小于 `0.01 mm`（host test）。
2. 起点、抬高点、前摆落点和 `0..100%` 幅度的所有 20 ms 采样点均在两连杆可达域内。
3. 完整幅度相邻 20 ms 采样的髋/膝角度变化均小于 `0.05 rad`。
4. 幅度为 `0%` 时足端保持起点；`100%` 时到达 `(30, -100) mm`。
5. 实体测试前必须另行完成关节零位、方向、止挡、电流、温升和足底实际位置标定。
6. 在实体数据不足前，不把本模块输出转换成正式执行器脉宽，也不替换现有 `step rf`。

## Software evidence

- 新增 `firmware-esp32/tools/servo-zero-test/main/servo_zero_leg_math.[ch]`。
- `servo_zero_leg_fk` / `servo_zero_leg_ik` 实现两连杆 FK/IK；`servo_zero_leg_lift_pose`
  输出足端点、关节角、可达性和完成状态。
- `servo-zero-test` 的 host C 测试覆盖 FK/IK 往返、不可达点、完整幅度、半幅度以及
  每 `20 ms` 采样的可达性。
- 构建版本：`489673b-dirty`；本次工作发生在未提交共享工作区。

## Result

- 严格 C 测试：
  `cc -std=c11 -Wall -Wextra -Werror -Ifirmware-esp32/tools/servo-zero-test/main firmware-esp32/tools/servo-zero-test/main/servo_zero_math.c firmware-esp32/tools/servo-zero-test/main/servo_zero_leg_math.c firmware-esp32/tools/servo-zero-test/tests/test_servo_zero_math.c -lm -o /tmp/niulai-servo-zero-math-test && /tmp/niulai-servo-zero-math-test`，通过。
- 正、反两条膝关节 IK 支路均完成 FK/IK 往返和完整轨迹可达性测试；实体装配使用哪一支路仍未冻结。
- 100% 幅度、20 ms 采样的模型最大关节角变化为髋 `0.02966 rad`、膝 `0.04415 rad`，均低于
  `0.05 rad` 试验阈值；这是模型结果，不是实体速度测量。
- Python 固件测试：14 项通过；工具测试：5 项通过。
- `firmware-esp32/tests/host/run_host_tests.sh`：全部通过。
- ESP-IDF v5.5.5 `idf.py -B build-servo-zero build`：通过；镜像大小 `0x43c40`，应用分区
  余量 74%。
- 实体重复次数：0；原始 UART 日志/视频：无。本次没有烧录或驱动执行器，因此没有实体
  失败数据可供推断。

## Next step

在实体安全条件满足后，先以 `25%` 幅度逐关节记录实际角度/脉宽对应关系和足端位置；
若方向、足端离地和电流均稳定，再比较 `50%`、`75%`、`100%`。保留失败记录，不从
UART 的轨迹完成日志推断动作自然或安全。
