# EXP-002 PCA9685 八通道 ASCII 调零

## Objective

验证 ESP32-S3 通过 PCA9685 持续输出八路舵机中位候选脉冲，并提供可复现的 UART ASCII 调整入口。

## Hypothesis

PCA9685 的 CH0--CH7 可分别绑定四条腿的髋/膝关节；默认 1500 us 作为拆下舵机臂时的中位候选，CH8--CH15 保持 FULL_OFF。

## Hardware version

- ESP32-S3 N16R8，USB Serial/JTAG，测试端口 `/dev/ttyACM0`。
- PCA9685 16 路模块，默认 I²C 地址 `0x40`。
- Carrier 当前接线：SDA=GPIO11、SCL=GPIO12、OE=GPIO1、VCC=3.3 V；舵机 V+ 使用独立受控约 6 V 电源，共地。
- 关节映射：CH0 右前髋、CH1 右前膝、CH2 左前髋、CH3 左前膝、CH4 右后髋、CH5 右后膝、CH6 左后髋、CH7 左后膝。

## Software commit

原始记录基于任务分支 `feat/android-offline-test-mode` 的未提交工作区；应用版本字符串为当时的 `d001cd7-dirty`。后续验证同样使用未提交工作区修改，复现时应以实际 `git status` 和当前 commit 为准。独立项目位于 `firmware-esp32/tools/servo-zero-test/`。

## Parameters

- PWM：50 Hz；默认 1500 us。
- ASCII 测试软件范围：500--2500 us；`angle` 使用名义 1000--2000 us 对应 180° 换算，实际角度需实测。扩展范围仅用于逐通道测量机械止挡，不代表安全动作范围。
- 刷新周期：500 ms。
- `/OE` 初始化保持 HIGH；只有收到明确的 `run` 后才拉 LOW。
- 当前已恢复首次成功 CH0 固件的初始化顺序：MODE1 SLEEP+AI → MODE2 OUTDRV → PRESCALE → 未使用通道 FULL_OFF → 活动通道预载 → MODE1=RESTART+AI 唤醒，并等待 10 ms。

## Test method

1. 拆下舵机臂、固定舵机和 PCA9685，确认保险丝、6 V 限流和急断电条件。
2. 构建：

   ```bash
   cd firmware-esp32/tools/servo-zero-test
   idf.py -B /tmp/niulai-servo-zero-build build
   ```

3. 在同一硬件权限上下文创建 `/dev/ttyACM0`（WSL 环境可能只在 `/sys/class/tty/ttyACM0` 出现），烧录并硬复位。
4. UART 发送（先 `run`，结束时 `stop`）：

   ```text
   select 0
   pulse 1400
   pulse 1600
   center
   run
   stop
   ```

5. 重复一次启动与 UART 命令回执检查；记录是否有 I²C 错误、舵机动作、电流、抖动和温升。测量止挡时从当前值以 1 us 或 10 us 小步单通道调整，碰到止挡、抖动、异常电流或发热立即发送 `stop` 并切断 6 V。

## Video / logs

本次启动和 UART 原始输出保存在共享工作区的 `/tmp/niulai-servo-zero-build/log/`；关键日志显示 `CH0-CH7 ENABLED`、`ASCII calibration ready` 以及 CH0 的 1400/1600/1500 us 回执。未保存照片或视频。

## Result

- 首次全通道版本启动失败，日志为 `cannot put PCA9685 to sleep`，随后 `/OE remains HIGH forever`。
- 检查发现 PCA9685 的 SDA 线脱落；重新接回控制排针 SDA（GPIO11）后，重新烧录版本成功初始化 PCA9685。
- UART 已确认 CH0--CH7 启用、CH8--CH15 FULL_OFF；CH0 的 1400 → 1600 → 1500 us 命令均收到回执，未再出现 I²C 错误。
- UART 回执证明 I²C 写事务成功，但不等于已完成实体舵机机械动作验收；实体运动、电流和零位仍待 Operator 观察记录。

## Follow-up verification (2026-09-11)

- 独立舵机固件加入 I²C 地址扫描和逐通道错误日志后重新烧录；UART 在 SDA=`GPIO11`、SCL=`GPIO12`、地址 `0x40` 下发现 `I2C device ACK at 0x40`，扫描到 1 个设备。
- UART 随后显示 `CH0-CH7 READY at 50 Hz` 和 `/OE HIGH`。本轮没有发送 `run`，没有驱动实体舵机；这只证明 PCA9685 初始化和总线 ACK，不证明实体动作或机械零位。
- 随后修正通道帧初始化：在 PCA9685 MODE1 的睡眠与唤醒状态均保持 `AI`（auto-increment）置位，使每个五字节通道帧连续写入 `ON_L/ON_H/OFF_L/OFF_H`；唤醒使用早期已验证的 `RESTART|AI`，独立 ESP-IDF 构建和烧录通过。
- 本次 UART 原始输出未另存为独立日志文件；下一轮应在拆臂、固定和限流条件满足后保存原始日志，并记录逐通道动作、电流、限位和零位。
- 修复版 `973987d` 重新烧录后，Operator 实际观察到 CH0 在 `1400 → 1600 us` 的有界测试中动作，随后 UART 确认 `stop -> /OE HIGH`。该结果验证了 PCA9685 PWM 输出链路至少对 CH0 可用；尚未完成八路逐通道零位、方向、电流和机械限位验收。
- 为测量真实机械止挡，将电脑端和 ESP32 固件软件测试范围从 `1100..1900 us` 扩展为 `500..2500 us`。使用构建元数据指定的 ESP-IDF v5.5.5 环境重新构建并烧录到 `/dev/ttyACM0`；bootloader、partition table、app 三段均报告 `Hash of data verified`，并完成硬复位。启动日志确认 `I2C device ACK at 0x40`、`CH0-CH7 READY at 50 Hz`、`/OE HIGH`。本次未发送 `run`，未进行新的实体动作测量。

## Operator measurement (2026-09-12)

Operator 按前一条记录的顺序报告了四个髋关节的当前脉宽：

| Channel | Joint | Reported pulse |
| --- | --- | ---: |
| CH0 | RF hip | 1400 us |
| CH2 | LF hip | 1750 us |
| CH4 | RR hip | 1940 us |
| CH6 | LR hip | 1120 us |

本表先作为实测记录保存；本次消息未注明每个值对应的止挡方向，也未提供重复次数、电流或温升，因此不将其冻结为最终机械限位。

2026-09-12：在 Operator 明确授权单通道探针后，发送整帧
`1600,1469,1427,1415,1619,1582,1449,1600 us`，仅将 CH0 从当前直立值改为
`1600 us`，随后 `run` 保持约 2 s、`stop` 拉高 `/OE`，最后 `load` 恢复保存配置。UART
回显完整且无 I²C 错误。Operator 观察 CH0 向后约 `10°`；这给出当前中立点附近约
`14.6 us/°` 的局部脉宽斜率（`146 us / 10°`），但只有一个采样点，不能冻结为最终标定。
CH0 的 `step rf 100` 目标 `1680 us` 按此局部估计仅约 `4.5°`，可解释实体基本没有抬腿；
电流、温升和机械止挡仍待记录。

2026-09-12：在 Operator 明确授权后执行 CH1 单通道探针，发送整帧
`1746,1360,1427,1415,1619,1582,1449,1600 us`，保持约 2 s 后 `stop`、`load` 收尾。
UART 回显完整且无 I²C 错误。Operator 观察 CH1 向前弯约 `10°`；当前中立点附近可暂估
`10.9 us/°` 的局部脉宽斜率（`109 us / 10°`），但只有一个采样点，不能冻结为最终标定。
电流、温升和机械止挡仍待记录。

## Next step

Operator 在舵机臂拆除状态下观察 CH0 动作，再逐个使用 `select <channel>` 和 `pulse/us/ms/angle` 调整机械零位；每个通道保存前记录原始中位脉宽和方向。完成后补充照片/视频、电流与限位结果，并由非作者人类 review 实体动作变更。
