# EXP-021 直连 GPIO、端口修正后的 50 轮舵机测试

## Objective

确认修正 PCA9685 接线端口、共地并恢复 `0x40` ACK 后，8 路舵机回中动作在
400 kHz I²C 下的 NACK 情况。

## Configuration

- ESP32-S3；USB 供电；PCA9685 逻辑侧与 ESP32 共地。
- SDA=`GPIO11`、SCL=`GPIO12`、`/OE`=`GPIO1`；PCA9685 地址 `0x40`。
- I²C=`400 kHz`，动作命令为 `pose pseudo 35`，每轮随后发送 `stop`。
- 50 轮；通过宿主机 PowerShell 的 `COM4` 执行；未接入 MPU6050。
- 测试前启动扫描已确认 `0x40` ACK；测试前未发现初始化错误。

## Result

50/50 轮均收到 `rest pose reached` 并成功返回 `/OE HIGH`。最终统计：

```text
logical=4833 attempts=4834 success=4833 nack=1 timeout=0 other=0
retries=1 recovered_after_retry=1 terminal_failures=0
```

NACK 发生 1 次，重试后恢复；没有 software timeout、最终失败或未完成轮次。

## Evidence

- 原始串口日志：[EXP-021-direct-gpio-correct-port-50-rounds.log](logs/EXP-021-direct-gpio-correct-port-50-rounds.log)
