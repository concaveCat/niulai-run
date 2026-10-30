# EXP-014 CRC UART transport and shared-I²C isolation

## Status

Testing：CRC/ACK 串口路径已完成软件验证和固件烧录；实体总线验证被 PCA9685/MPU6500
启动时同时无 ACK 阻塞。舵机输出保持 `/OE HIGH`。

## Objective

避免 ASCII 串口命令在重连、分包或编码环境下被误接受，并区分 UART 传输故障与 PCA9685、
MPU6500 共享 I²C 的电气故障。

## Protocol

目标帧为固定 24 字节：`SOF(0xa5), version, command, sequence, mask, 8×uint16 pulse,
CRC16-CCITT`。控制帧为固定 8 字节；设备只对 CRC、版本、命令、通道掩码和 `500..2500 us`
范围检查通过的帧回 ACK/NACK。`set-target` 不启用 `/OE`；`run` 只有完整八通道目标已确认后才
允许；`stop` 始终拉高 `/OE`。主机工具在打开端口后等待启动、匹配序号并校验 ACK CRC。

## Verification

- `python3 -m unittest tools.tests.test_servo_uart_protocol -v`：4/4 通过。
- `git diff --check`：通过。
- ESP-IDF v5.5.5 isolated servo-zero build：通过，app 镜像 `0x49700`，分区余量 71%。
- 诊断版通过 COM4 烧录，bootloader、app、partition table 均 `Hash of data verified`。
- 启动日志捕获 `I2C scan found no ACK`、`MODE1 SLEEP write failed: ESP_ERR_INVALID_STATE`，
  固件随后保持 `/OE HIGH`；因此 CRC 帧没有收到 ACK，未执行 `run`。

## Next step

在输出关闭时断开 MPU6500 的 SDA/SCL 或电源，只保留 PCA9685，确认 `0x40` ACK、SDA/SCL
空闲高电平、上拉和公共地；再单独接回 MPU6500 并确认 `0x68` ACK。两者同时无 ACK 时，
不得把问题归因于 UART，也不得重新执行大幅动作。

## Risk and rollback

协议回滚为旧 ASCII 命令，但旧命令不得用于大幅动作。I²C 故障时 `/OE HIGH` 是默认安全状态；
不得通过任意擦除或猜测烧录 offset 排障。
