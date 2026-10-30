# EXP-019 USB 拔除条件下 Wi‑Fi TCP 诊断 400 kHz 50 轮

## Objective

验证在不连接 USB、通过 ESP32 SoftAP/TCP 获取日志时，8 个舵机的 `pose pseudo 35`
基准动作是否仍出现 I²C NACK 或 software timeout。

## Hardware and software

- ESP32-S3 rev 0.2；PCA9685 地址 `0x40`；Gerber 映射 SDA=`GPIO11`、SCL=`GPIO12`、
  `/OE`=`GPIO1`。
- 按本轮既定条件：USB 拔除、外部 3.3 V、Servo 6 V 接通、8 个舵机连接、MPU6050
  断开；I²C=`400 kHz`。
- ESP-IDF v5.5.5；隔离 `servo-zero-test` 工作区镜像；通过 SoftAP
  `牛来快跑-DIAG` / TCP `192.168.4.1:3333` 获取日志。

## Parameters and method

- 主机自动执行 50 轮 `pose pseudo 35`。
- 每轮等待 `rest pose reached`，随后发送 `stop`，等待 `/OE HIGH`。
- 运行前发送 `stats reset`，运行结束发送 `stats`。
- 统计定义：`logical` 是逻辑 I²C transaction，`attempts` 包含重试的物理尝试；
  `nack`、`timeout`、`other` 按失败尝试计数，`terminal_failures` 是重试后仍失败的
  transaction。

## Operator result

Operator 回报最终日志：

```text
I2C stats: logical=4796 attempts=4796 success=4796 nack=0 timeout=0 other=0 retries=0 recovered_after_retry=0 terminal_failures=0
```

因此本次回报的 4796 次逻辑 transaction 全部一次成功；NACK、timeout、其他错误和
最终失败均为 0。Operator 未贴出完整逐轮原始日志，因此本记录以最终 `stats` 为证据，
50 轮完成标记和每轮日志尚未独立归档。

## Conclusion

在上述 Wi‑Fi 诊断条件下，当前样本没有复现 I²C NACK。这支持“该故障依赖供电/USB/
射频/线束等运行条件”的判断，但不能单独证明 USB 双电源、舵机瞬态或串扰中的某一个
是根因；Wi‑Fi 本身也改变了 ESP32 的射频活动和功耗。该结果不能与 USB 条件的统计直接
合并，仍应结合示波器对 Servo 6 V、3.3 V、公共地、SDA 和 SCL 的同步观测。

## Evidence

- 原始证据：Operator 在 2026-09-12 对话中贴出的最终 `I2C stats` 行。
- 完整 TCP 输出未归档；若保留在主机日志文件，应补充其路径。
