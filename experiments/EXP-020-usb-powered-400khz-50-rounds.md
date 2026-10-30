# EXP-020 ESP32 USB 供电对照：PCA9685 启动 ACK 检查

## Objective

将 ESP32 开发板改为由电脑 USB 供电，保持 PCA9685/舵机侧接线和共地，验证此前
Wi‑Fi 条件下无 NACK 的结果是否能在 USB 供电条件下复现。

## Hardware and software

- ESP32-S3 rev 0.2；PCA9685 目标地址 `0x40`；SDA=`GPIO11`、SCL=`GPIO12`、
  `/OE`=`GPIO1`。
- Operator 确认已共地；ESP32 由电脑 USB 供电。PCA9685 逻辑 VCC 到底是否在器件
  引脚处保持 3.3 V，本轮未单独测量记录。
- ESP-IDF v5.5.5，隔离 `servo-zero-test`，I²C 配置为 `400 kHz`；宿主机串口为
  `COM4`。

## Method

1. 使用已核对的 `flasher_args.json` 重新烧录 bootloader、partition table 和 app，
   三个镜像均由 esptool 报告 `Hash of data verified`。
2. 通过 USB 串口启动，默认保持 `/OE HIGH`。
3. 计划执行 50 轮 `pose pseudo 35 → stop`；若启动扫描失败，立即停止，不发送动作。

## Result

测试在启动阶段终止，未执行任何一轮动作。串口原始日志为：

```text
I2C scan found no ACK; check pull-ups, 3.3V, GND, SDA=11, SCL=12
register write I2C failed after 4 attempts: ESP_ERR_INVALID_STATE (0x103)
MODE1 SLEEP write failed: ESP_ERR_INVALID_STATE (0x103), SDA=11 SCL=12 addr=0x40
initialization failed; /OE remains HIGH forever
```

启动阶段已经有 `4` 次寄存器写尝试且全部 NACK；由于 PCA9685 初始化失败，固件没有
创建可用的动作/命令流程，50 轮统计不适用，舵机没有被本次测试启动。

## Conclusion

在当前 USB 供电接线下，问题不是“动作期间 NACK 率”，而是 PCA9685 在启动时完全
不可见。最优先检查 PCA9685 器件引脚处的 `VCC-GND` 电压、PCA9685 `GND` 回路、
SDA/SCL 是否接到了同一组 GPIO，以及 SDA/SCL 上拉是否连接到有效的 3.3 V。只有
重新看到 `0x40 ACK` 和 `CH0-CH7 READY` 后，才有意义继续做 50 轮动作对照。

## Follow-up: direct GPIO wiring

Operator subsequently connected the I²C wires directly to the ESP32 GPIO header
and confirmed common ground. The bootloader, partition table, and application
were then reflashed successfully (all three reported `Hash of data verified`).
A fresh reset-only probe was run with the reflashed firmware and no motion
command. The result was still:

```text
I2C bus is still busy but software timeout detected
I2C scan found no ACK; check pull-ups, 3.3V, GND, SDA=11, SCL=12
... unexpected nack detected ... (4 attempts)
MODE1 SLEEP write failed: ESP_ERR_INVALID_STATE (0x103), SDA=11 SCL=12 addr=0x40
initialization failed; /OE remains HIGH forever
PROBE_ACK=NONE
```

This confirms that the direct GPIO connection did not restore a valid ACK at
the configured PCA9685 address `0x40`. The bus-busy timeout is also present, so
this is not yet classifiable as a simple address mismatch. No servo motion was
sent.

After Operator corrected the common-ground connection and requested a repeat,
the reset-only probe produced no `probe device timeout`/bus-busy lines, but
still reported `I2C scan found no ACK` and four failed writes to configured
address `0x40` (`ESP_ERR_INVALID_STATE`, unexpected NACK). No servo motion was
sent. This separates the earlier bus-busy symptom from the remaining address/
signal/connection failure; a valid `0x40` ACK is still absent.

After Operator corrected the connected port, the next reset-only probe reported
`I2C device ACK at 0x40 (configured PCA9685)`, additionally saw `0x70`, and
entered `CH0-CH7 READY at 50 Hz PWM / 400000 Hz I2C`. No servo motion was sent.

## Evidence

- 原始串口日志：[EXP-020-usb-powered-400khz-50-rounds.log](logs/EXP-020-usb-powered-400khz-50-rounds.log)。
- 烧录日志：本次宿主机 esptool 输出；三个镜像均完成 hash 校验。
