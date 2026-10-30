# EXP-009 四膝大幅往返 demo（历史基线 ±80°，当前修订 ±96°）

## Status

Testing：这是隔离 PCA9685 测试工具中的 HIGH RISK 探索命令，不接 BLE、FSM 或正式
执行器 adapter。当前只完成软件实现和离线验证，尚未进行实体动作验收。

## Objective

让四个髋关节保持当前垂直中立位，同时让四个膝关节在前后方向做大幅度往返，验证
多通道整帧输出、方向映射、平滑轨迹和紧急 `stop` 关断路径。

## Hypothesis

在当前“名义 `1000..2000 us = 180°`”换算下，以中立位为中心的当前修订 `±96°`
（峰峰值约 `192°`）脉宽轨迹可以由 PCA9685 每 20 ms 的整帧更新平滑执行。左右镜像膝关节使用
相反的脉宽方向后，四条腿应共享同一物理前后方向；该方向仍需逐腿观察确认。

## Hardware version

- ESP32-S3 N16R8 + PCA9685，地址 `0x40`，PWM 50 Hz。
- 当前 Carrier 候选接线：SDA=`GPIO11`、SCL=`GPIO12`、`/OE`=`GPIO1`，舵机使用独立
  受控约 6 V 电源并共地。
- CH0/2/4/6 为髋，CH1/3/5/7 为膝；CH8--CH15 为 `FULL_OFF`。
- 膝关节没有机械限位；实体运行前必须有可靠机械固定、人员净空、6 V 限流、保险丝
  和可立即断电的急停。

## Software commit

当前为 `fw/kw10-perfect-touchdown` 分支的未提交 exploratory work；以实际工作区状态
和后续 MR commit 为准。实现位于 `firmware-esp32/tools/servo-zero-test/`。

## Parameters

| 项目 | 值 |
| --- | --- |
| 髋中立 CH0/2/4/6 | `1746/1427/1619/1449 us` |
| 膝中立 CH1/3/5/7 | `1469/1415/1582/1600 us` |
| 名义摆幅（当前修订） | `±96°`，`PCT=100`；`PCT=25` 约为 `±24°` |
| 名义每侧脉宽偏移 | `533 us` |
| 中立稳定 | `500 ms` |
| 首次进入端点 | `760 ms` |
| 后续每半周期 | `760 ms` |
| 更新周期 | `20 ms`，由 ESP32 绝对节拍驱动，每次一条 CH0--CH7 整帧 |
| 膝脉宽方向（当前探针） | CH1/CH3/CH5/CH7 增大；CH1/CH3 为本轮反向（装配假设） |

当前 `PCT=100` 的软件端点为：CH1/3/5/7=`936/882/1049/1067 us`，另一侧为
`2002/1948/2115/2133 us`。这些值只由名义换算得到，不是机械角度或安全限位。

## Acceptance criteria

1. host 数学测试覆盖中立、两端点、往返周期、髋固定、端点软件范围和低幅度模式。
2. ESP-IDF build 通过；固件启动默认 `/OE HIGH`，不会自动启动 demo。
3. 发送 `demo knee-swing [PCT]` 时只改变膝脉宽，髋保持中立；任一 I²C 错误使
   `/OE HIGH` 并保留中立目标。
4. 发送 `stop` 立即拉高 `/OE`，并将内存目标恢复到四髋/四膝中立值。
5. 实体验收还必须由 Operator 观察方向、平滑性、是否撞击/拖地、6 V 电流/压降、噪声、
   温升和急停；软件日志不能替代这些证据。

## Test method

软件阶段运行：

```bash
cd firmware-esp32/tools/servo-zero-test
cc -std=c11 -Wall -Wextra -Werror \
  main/servo_zero_math.c main/servo_zero_leg_math.c tests/test_servo_zero_math.c -lm \
  -o /tmp/niulai-servo-zero-math-test
/tmp/niulai-servo-zero-math-test
```

ESP-IDF build（只构建，不烧录）：

```bash
env IDF_TOOLS_PATH=/tmp/niulai-idf-tools IDF_PATH=/tmp/niulai-esp-idf-v5.5.5 \
  /tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env/bin/python \
  /tmp/niulai-esp-idf-v5.5.5/tools/idf.py -B build-servo-zero build
```

确认实体安全条件后，先在舵机臂拆除或单腿受控固定状态，以 `demo knee-swing 25` 开始；
无异常再逐步提高到 `50`、`100`。每次动作保持串口可发送 `stop`，发现方向错误、无机械
余量、异响、过流、掉压、温升或任何失控迹象时立即 `stop` 并切断 6 V。每个幅度至少
记录一次原始 UART、实际脉宽、电流/压降、动作视频和失败结果。

## Video / logs

当前无实体视频或动作日志；软件测试输出由本次验证命令产生，未作为实验原始硬件证据
归档。

## Result

之前烧录的 `3000 ms` 版本已在 `/dev/ttyACM0` 完成一次有界 100% 串口运行：启动日志
确认 PCA9685 `0x40` ACK、50 Hz、`CH0-CH7 READY` 和 `/OE HIGH`；约 10 s 监听期间没有
NACK、重启或错误，随后 `stop` 确认 `/OE HIGH`。本次结果没有视频、电流、温升或机械
测量，UART 不能证明实际方向和角度。

当前速度版本已完成重新 build/烧录/有界运行：应用大小为 `0x44a90`，三段镜像均
`Hash of data verified`；启动后发送 `demo knee-swing`，UART 明确报告每半摆 `1500 ms`
和 `ESP32 20 ms absolute cadence`，约 13.5 s 运行期间无 NACK、重启或错误，随后
`stop -> /OE HIGH`。这只证明软件调度和串口/I²C 路径完成；本轮仍没有视频、电流、温升
 或机械测量，不能把 `PCT=100`、`±80°` 或“更平滑”描述为已通过的实体动作质量。

当前高速修订已完成 host 数学测试、ESP-IDF v5.5.5 build 和按 `flasher_args.json` 的
三段烧录，应用大小为 `0x44aa0`，三段均 `Hash of data verified`。在 Operator 确认安全
条件后执行 `demo knee-swing 100` 约 10 s：UART 报告名义 `±96°`、端点
`A=2002/882/2115/1067 us`、`B=936/1948/1049/2133 us`、每半摆 `760 ms`；期间无
NACK、重启或错误，随后 `stop -> /OE HIGH`。`760 ms` 是为对齐 ESP32 的 20 ms 节拍而取的
38 帧值，约为原速度的 1.97 倍；新软件端点仍落在 `500..2500 us` 传输范围内，但该范围
不是机械限位。本轮仍没有归档视频、电流或 6 V 压降波形，因此 UART 结果不能单独证明
实体动作平滑性、方向、机械余量或供电裕量。

随后按 Operator 要求反向 CH3 和 CH5 的脉宽方向，方向表改为 `CH1/3/5/7=-/-/+/+`。
host 数学测试和 ESP-IDF build 通过后重新烧录；100% 演示约 10 s，UART 确认端点
`A=2002/1948/1049/1067 us`、`B=936/882/2115/2133 us`，无 NACK、重启或错误，随后
`stop -> /OE HIGH`。本轮仍没有视频、电流或 6 V 压降波形，CH3/CH5 的实际物理方向
仍需 Operator 观察确认。

随后按 Operator 要求再反向 CH1 和 CH3，方向表改为 `CH1/3/5/7=+/+/+/+`。本轮重新
通过 host 数学测试和 ESP-IDF build 后烧录；100% 演示约 10 s，UART 确认端点
`A=936/882/1049/1067 us`、`B=2002/1948/2115/2133 us`，无 NACK、重启或错误，随后
`stop -> /OE HIGH`。本轮仍没有视频、电流或 6 V 压降波形，四个膝关节的实际物理方向
仍需 Operator 观察确认。

## Next step

下一步由 Operator 回填 `760 ms / ±96°` 版本的左右膝方向、实际平滑性和端点余量，并记录
实际角度/限位、电流、6 V 压降、温升和急停数据；
在此之前不得把该命令移入正式驱动或 BLE 路径。
