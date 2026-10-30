# EXP-022 调整接线后的 PCA9685 ACK 探测

## Setup and method

Operator调整了 I²C 接线；使用当前已烧录固件（SDA=`GPIO11`、SCL=`GPIO12`、
PCA9685 地址 `0x40`、I²C=`400 kHz`）通过宿主机 `COM4` 复位并读取启动日志。
只做启动探测，没有发送舵机动作。

## Result

本次未收到 `0x40` ACK。扫描期间反复出现 `probe device timeout` 和
`I2C transaction timeout detected`，最后为：

```text
I2C scan found no ACK
MODE1 SLEEP write failed: ESP_ERR_INVALID_STATE (0x103), SDA=11 SCL=12 addr=0x40
initialization failed; /OE remains HIGH forever
PROBE_ACK=NONE
```

相对于 EXP-021 的已知良好接线，本次调整后的接线未通过启动 ACK 检查，并使总线
出现持续超时；没有进行 50 轮动作测试。

随后 Operator 仅将 SDA 改为直连并重复同样探测。结果仍为大量总线/探测超时、
`I2C scan found no ACK` 和 `PROBE_ACK=NONE`；因此仅旁路 SDA 尚不足以恢复通信，
SCL 路径、两线映射或公共回路仍需继续检查。没有进行动作测试。
