# EXP-017 20 kHz 共享 I²C 与 IMU 接入验证

## Objective

验证当前 PCA9685 八路整帧路径在 `20 kHz` I²C、50 Hz PWM 和舵机供电接通时是否有足够余量，
并判断新增 IMU 是否可能通过共享 SDA/SCL 参与 NACK。

## Hardware version

- ESP32-S3 rev 0.2，CH340 `1a86:55d3`，宿主机原生 PowerShell `COM4`。
- Operator 确认实际 Gerber 映射：SDA=`GPIO11`、SCL=`GPIO12`、OE=`GPIO1`；PCA9685 地址为
  `0x40`。
- 启动扫描另有 `0x68` ACK。Operator 当前称该器件为 MPU6050；仓库驱动和历史接口表登记为
  MPU6500，具体芯片型号尚未由 `WHO_AM_I` 实测确认。
- 20 kHz 动作阶段舵机 6 V 已接通；第一阶段保留 IMU，第二阶段断开 MPU6050 的 I²C 接入；本轮没有同步保存电源最低电压或示波器波形。

## Software and parameters

- Git HEAD `8898101`，工作区 dirty；ESP-IDF v5.5.5。
- `CONFIG_SERVO_ZERO_I2C_SPEED_HZ=20000`，PCA9685 PWM=`50 Hz`。
- PCA9685 八路整帧为 33 字节（首字节为起始寄存器，随后 32 字节通道数据）；单次写入最多
  4 次，失败间隔 `1 ms`。
- `pose pseudo` 使用 20 ms 控制节拍；实验结束后重新构建、烧录并确认恢复到 400 kHz。

## Test method

1. 通过宿主机 PowerShell 烧录 20 kHz 镜像，复位读取总线扫描和 `READY`。
2. 执行 35% 幅度全帧轨迹 1 轮，随后重复相同条件 3 轮；每轮约 3.5--4.8 s 后发送 `stop`。
3. 在 Operator 明确授权后执行 100% 幅度全帧轨迹 1 轮，约 3.5 s 后发送 `stop`。
4. 断开 MPU6050 后重新烧录同一 20 kHz 镜像，确认扫描不再看到 `0x68`；舵机保持供电，重复 4 轮 35% 全帧轨迹，每轮自动 `stop`。
5. 通过 PowerShell 烧录恢复后的 400 kHz 镜像，复位确认 `/OE HIGH`。

## Result

- 20 kHz 启动成功：`0x40` 和 `0x68` 均 ACK，报告 `CH0-CH7 READY at 50 Hz PWM / 20000 Hz I2C`，
  初始化阶段 `/OE HIGH`。
- 35% 全帧轨迹共 4 轮，其中 3 轮出现至少一次 active-frame 硬件 NACK；失败均为某帧的第
  `1/4` 次尝试，重试后轨迹继续并到达 `rest pose reached`。4 轮均以 `stop` 收尾为 `/OE HIGH`。
- 100% 全帧轨迹出现至少 8 次 active-frame 第一次尝试 NACK；有界重试后仍到达静息位，随后
  `stop` 收尾为 `/OE HIGH`。没有出现连续 4 次失败导致动作自动中止。
- 断开 MPU6050 后，启动扫描只发现 `0x40`，不再发现 `0x68`；同一 20 kHz、舵机供电和 35%
  幅度条件下，4 轮中前 3 轮无 NACK，第 4 轮仍出现 3 次 active-frame 首尝试 NACK。该结果
  证明 MPU6050 不是触发 NACK 的必要条件。
- 恢复烧录后，启动再次看到 `0x40`、`0x68` ACK，并报告 `50 Hz PWM / 400000 Hz I2C`、`/OE HIGH`。
- 后续在 MPU6050 断开、舵机 6 V 接通、400 kHz 且大电流供电线与杜邦线束空间分离的条件下，重复 `pose pseudo 35` 20 轮：第 6、9 轮各出现 1 次 active-frame 首尝试 NACK，重试均恢复；其余 18 轮无 NACK，20 轮均到达静息位并以 `stop` 收尾为 `/OE HIGH`。该结果作为线束并拢前的 400 kHz 基线；尚未证明空间分离带来改善。
- 随后在相同条件下将大电流供电线与杜邦线束并拢，再重复 20 轮：第 3 轮出现 1 次 `I2C software timeout`，第 4 轮出现两次连续硬件 NACK 后恢复；其余 18 轮无 I²C 错误，20 轮均到达静息位并以 `stop` 收尾为 `/OE HIGH`。受影响轮次仍为 2/20，与空间分开时相同；但并拢条件出现了更严重的单轮重试序列，因此线束串扰仍是未决假设，不能由本轮单独确认。

## 20 kHz feasibility calculation

I²C 线上还要发送 PCA9685 的 1 个地址字节，因此一帧至少约为
`(1 + 33) × 9 = 306` 个 SCL 周期。理想线占用时间为：

| I²C 速率 | 理想整帧线占用 | 20 ms 周期余量（未计软件开销） |
| ---: | ---: | ---: |
| 100 kHz | 3.06 ms | 16.94 ms |
| 20 kHz | 15.30 ms | 4.70 ms |
| 10 kHz | 30.60 ms | 不可能每 20 ms 完成整帧 |

START/STOP、驱动调度和实际波形会进一步减少余量；此前 100 kHz 约 3.2 ms 的 trace 与该估算一致。
因此 20 kHz 对“偶尔发送一帧”是可行的，对当前“每 20 ms 发送八路原子整帧”仅在无任何
NACK、无其他 I²C 访问时勉强可行；一次重试就会再增加约 `15.3 ms + 1 ms`，立即破坏周期。
半帧约为 `(1 + 17) × 9 / 20 kHz = 8.1 ms`，但每个通道只能每 40 ms 更新一次，且不再是
八路原子更新。

## Conclusion

20 kHz 不是当前全帧玩法的可靠工作点。`0x68` 设备与 PCA9685 在同一总线上是合理的多从机
配置，不构成地址冲突；IMU 模块若接在 GPIO11/12 的 I²C 线上，仍可能增加线电容、杜邦线耦合和
额外上拉网络，但 A/B 结果表明它不是本次 NACK 的必要条件。当前隔离固件没有轮询 IMU，不存在
IMU 与 PCA9685 争抢主机事务的问题。无 MPU6050 时仍出现 NACK，结合舵机供电下陷/公共地瞬态
观测，当前优先级应转向舵机供电、公共地、舵机线束和机械负载；1 MHz 无舵机时的失败仍说明
共享 I²C 物理层本身存在独立的高速余量问题。

## Evidence

- 原始串口摘要：[EXP-017-20khz-shared-i2c-with-imu.log](logs/EXP-017-20khz-shared-i2c-with-imu.log)。
- 当前总线配置：[servo-zero sdkconfig.defaults](../firmware-esp32/tools/servo-zero-test/sdkconfig.defaults)。
- 当前 PCA9685 总线扫描和 GPIO 配置：[servo_zero_main.c](../firmware-esp32/tools/servo-zero-test/main/servo_zero_main.c)。
