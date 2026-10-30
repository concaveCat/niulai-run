# EXP-015 400 kHz I²C 与有界重试的无舵机负载验证

## Objective

验证 PCA9685 共享 I²C 在舵机 6 V 断开时，以 400 kHz、每次最多四次尝试、相邻尝试间隔
约 1 ms 运行，是否仍出现 NACK；同时确认失败路径不会留下 `/OE LOW`。

## Hypothesis

提高 I²C 速率会缩短八路整帧占用时间；对短暂总线错误做有限重试可能提高一次动作提交的
成功率，但不能修复持续的供电下陷、公共地反弹或信号完整性问题。

## Hardware version

- ESP32-S3 rev 0.2，CH340 `1a86:55d3`，宿主机原生 PowerShell `COM4`。
- PCA9685 `0x40`、MPU6500 `0x68`；按 Operator 确认的 Gerber：SDA=`GPIO11`、
  SCL=`GPIO12`、OE=`GPIO1`。
- PCA9685 PWM=`50 Hz`；舵机 6 V 电源在整个实验中断开，未施加舵机负载；逻辑侧继续
  由 ESP32/板上 3.3 V 供电。

## Software commit

Git HEAD `8898101`，工作区 dirty（实验性分支 `fw/kw10-perfect-touchdown`）。运行镜像由
ESP-IDF v5.5.5 构建；构建和烧录均使用 `firmware-esp32/tools/servo-zero-test/`
的 `build-servo-zero/` 元数据，未猜测 offset。

## Parameters

- `CONFIG_SERVO_ZERO_I2C_SPEED_HZ=400000`。
- 单次 `i2c_master_transmit` 最多尝试 `4` 次；失败后等待 `1000 us` 再尝试。
- 连续四次失败后记录错误并由动作路径停用 `/OE`；不执行无限 reset/retry。
- `pose pseudo 100`：八路 33 字节整帧、20 ms 内部节拍、2520 ms 轨迹。
- `pose pseudo-split 100`：CH0--CH3 与 CH4--CH7 交替 17 字节半帧、20 ms 节拍、
  每通道约 40 ms 更新。

## Test method

1. 通过宿主机 PowerShell 在 COM4 烧录 bootloader、app 和 partition table；三个镜像均
   报告 `Hash of data verified`，随后硬复位。
2. 读取启动日志，确认两个 I²C 地址 ACK、`50 Hz PWM / 400000 Hz I2C`，再发送 `stop`。
3. 重复 5 次 `pose pseudo 100`；每轮约 3.6 s 后发送 `stop`。
4. 重复 5 次 `pose pseudo-split 100`；每轮约 3.6 s 后发送 `stop`。
5. 只统计固件的 `I2C failed`、`retrying`、`NACK`、`rest pose reached` 和 `UART stop`。
   全部轮次结束后再次发送 `stop`。

## Video / logs

- 原始串口摘要：[EXP-015-400khz-i2c-bounded-retry-no-load.log](logs/EXP-015-400khz-i2c-bounded-retry-no-load.log)。
- 无视频；未测量本轮舵机电源电压，因为舵机 6 V 保持断开。

## Result

- 启动扫描得到 PCA9685 `0x40` 和 MPU6500 `0x68` ACK，并报告 `50 Hz PWM / 400000 Hz I2C`。
- 全八路整帧：`5/5` 轮启动、到达 `rest pose reached`、未出现 `I2C failed`/NACK，均由
  `stop` 收尾为 `/OE HIGH`。
- 交替半帧：`5/5` 轮启动、到达 `rest pose reached`、未出现 `I2C failed`/NACK，均由
  `stop` 收尾为 `/OE HIGH`。
- 本实验没有触发失败，因此没有直接验证四次重试分支；它证明的是在无舵机负载下，400 kHz
  实际总线和 20 ms 更新路径稳定。结合此前舵机供电接入时最低约 `5.7 V`、旧电源曾降至
  `2.4 V` 的观测，不能把本结果解释为已排除带负载时的电源/地回路问题。

## Next step

在机械固定、限流、人员净空和急停条件再次确认后，才恢复受控舵机电源；同时在 PCA9685
VCC、舵机输入、ESP32 3.3 V、SDA/SCL 和公共地附近同步测量。若 400 kHz 下带负载仍 NACK，
比较 NACK 时间与各电源轨下陷/总线波形，再决定降速、增加去耦/上拉/串联阻尼或调整供电回路。
