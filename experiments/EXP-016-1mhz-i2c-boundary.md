# EXP-016 1 MHz I²C 速率边界验证

## Objective

在舵机 6 V 断开的情况下，将 PCA9685 总线从已验证的 400 kHz 提高到 1 MHz，确认当前
PCB、杜邦线和上拉网络能否完成初始化，并判断 NACK 是否来自舵机负载。

## Hypothesis

如果 1 MHz 在无舵机负载时仍失败，而 400 kHz 成功，则当前 I²C 物理层没有足够的时序余量；
可能因素包括杜邦线束的总线电容、并行线耦合、上拉过弱、地回路阻抗或板上连接质量。

## Hardware version

- ESP32-S3 rev 0.2，CH340 `1a86:55d3`，宿主机原生 PowerShell `COM4`。
- PCA9685 `0x40`、MPU6500 `0x68`；SDA=`GPIO11`、SCL=`GPIO12`、OE=`GPIO1`。
- 舵机 6 V 电源全程断开；测试没有机械运动和舵机电流负载。

## Software commit

Git HEAD `8898101`，工作区 dirty；ESP-IDF v5.5.5；I²C 写入最多四次、间隔 1 ms 的实验固件。
1 MHz 仅覆盖当前生成的 `sdkconfig`，代码默认仍为 400 kHz。实验结束后当前运行镜像已恢复
为 400 kHz。

## Parameters

- 1 MHz：`CONFIG_SERVO_ZERO_I2C_SPEED_HZ=1000000`，其余参数不变。
- 启动扫描保留 `0x40` 和 `0x68` 探测；随后执行 PCA9685 初始化寄存器写入。
- 1 MHz 启动复位重复 3 次，每次等待启动日志；每次均由 4 次有界重试处理初始化写入。

## Test method

1. 临时覆盖生成的 `sdkconfig` 为 1 MHz，使用 `build-servo-zero/CMakeCache.txt` 和
   `flasher_args.json` 对应的 ESP-IDF 构建元数据编译并烧录。
2. 每次通过 COM4 复位并读取 ACK、NACK、重试和 `READY` 日志。
3. 发现初始化失败后，不继续发送动作命令；否则“pose”只是在未初始化状态下产生的无效结果。
4. 将生成配置恢复为 400 kHz，重新编译、烧录并确认正常启动。

## Video / logs

- 原始串口摘要：[EXP-016-1mhz-i2c-boundary.log](logs/EXP-016-1mhz-i2c-boundary.log)。
- 无视频；舵机 6 V 未接通。

## Result

- 1 MHz 下三次启动均先看到 `0x40` 和 `0x68` 地址 ACK；随后 PCA9685 第一个寄存器写入
  在 `1/4`、`2/4`、`3/4`、`4/4` 连续报告 `ESP_ERR_INVALID_STATE`/unexpected NACK。
- 三次均未出现 `CH0-CH7 READY`，说明失败发生在初始化寄存器数据阶段，而非舵机动作阶段。
- 该失败在无舵机负载时稳定复现；因此 1 MHz 不能作为当前这组线束/上拉/PCB 连接的工作速率。
- 恢复 400 kHz 后，启动再次得到两个 ACK，并报告 `50 Hz PWM / 400000 Hz I2C`，`/OE HIGH`。

## Next step

当前默认保持 400 kHz。若要验证并修复物理层，应先用示波器或逻辑分析仪同时看 SDA、SCL、
3.3 V 和地参考，测量上升时间、过冲、振铃和高低电平余量；再逐项缩短/分离杜邦线、加强
上拉、加入合适串联阻尼并改善公共地。1 MHz 复测应先通过初始化，再谈整帧和带负载动作。
