# EXP-036 当前 servo-zero 镜像重烧与启动检查

## Objective

重新烧录当前 `servo-zero-test` 镜像，并确认设备在烧录后保持安全启动状态。

## Parameters

- Windows PowerShell / 原生 Python esptool 4.12.0；端口 `COM4`。
- ESP32-S3；`460800` baud；flash mode `dio`、`16MB`、`80m`。
- 使用 `build-servo-zero/flasher_args.json` 的 `0x0/0x8000/0x10000` 偏移。
- 烧录后仅监听启动日志，未发送 `rest`、`1234` 或其他运动命令。

## Result

烧录成功，bootloader、应用和 partition table 均报告 `Hash of data verified`，
并完成硬复位。应用启动报告 `a2ce962-dirty`。

启动时 PCA9685 `0x40` 未 ACK，四次寄存器写入均失败，随后报告：

```text
I2C scan found no ACK
MODE1 SLEEP write failed: ESP_ERR_INVALID_STATE (0x103)
initialization failed; /OE remains HIGH forever
PROBE_ACK=NONE
```

本轮没有舵机运动。当前不能进行步态测试；需要先确认 PCA9685 逻辑侧 3.3 V、
公共地、SDA/SCL、上拉和供电线迁移后的连接状态。此前无负载 50 轮的 ACK 结果
不能覆盖本次重新接线后的新状态。

Operator 确认上述条件后再次执行硬复位并监听启动，结果仍为 `0x40` 无 ACK、
四次初始化写入失败和 `/OE remains HIGH forever`。因此该现象不是一次性的
复位状态残留，当前仍不能发送动作命令。

## Evidence

- [EXP-036 boot log](logs/EXP-036-2026-09-14-reflash-boot.txt)
