# EXP-033 ESP32-S3 MPU6500 I²C 可达性检查

## Objective

确认 MPU6500 是否能通过 ESP32-S3 的共享 I²C 引脚访问，并读取其身份寄存器和一帧
加速度/温度/陀螺仪原始数据。此次不测试姿态算法或闭环控制。

## Hypothesis

若地址 `0x68` 或 `0x69` 能 ACK，且寄存器 `WHO_AM_I (0x75)` 返回 MPU6500 的
`0x70`，同时 14 字节数据读取成功，则 MPU6500 的供电、公共地、SDA/SCL 接线和
ESP32 I²C 主机基本可达。

## Hardware version

- ESP32-S3 QFN56 rev v0.2，8 MB PSRAM，16 MB flash；宿主机 Windows 原生 CH343
  串口 `COM4`。
- Operator 接入的 MPU6500 模块；本次探针尝试地址 `0x68`、`0x69`。
- 探针先将 `/OE` 置 HIGH；不启动麦克风、I²S、BLE、FSM、PCA9685 或舵机。

## Software commit

`674d9a5-dirty`（共享工作区未提交 exploratory work）。

## Parameters

- I²C master：`SDA=GPIO11`、`SCL=GPIO12`、400 kHz、I²C port 0，启用 ESP32 内部
  上拉配置。
- MPU6500 7-bit 地址：`0x68`、`0x69`；预期 `WHO_AM_I=0x70`。
- 只读寄存器：`0x75`（身份）和 `0x3B..0x48`（14 字节原始传感器帧）；单次传输
  超时 100 ms。不写 MPU6500 配置寄存器。
- 固件配置：`CONFIG_NIULAI_MPU6500_PROBE_TEST=y`，
  `CONFIG_NIULAI_VOICE_KEYWORD_TEST=n`，所有 I²S 测试关闭。

## Test method

1. 使用独立 `/tmp/niulai-mpu6500-probe` build 目录构建隔离探针；检查生成的
   `config/sdkconfig.h`、`CMakeCache.txt` 和 `flasher_args.json`，不执行全片擦除。
2. 通过宿主机 PowerShell/native esptool 4.12.0 将构建元数据中的 bootloader、app
   和 partition table 写入 `COM4`。
3. 复位并读取 UART 启动日志；探针自动尝试两个地址，输出 ACK、`WHO_AM_I` 和
   14 字节解码后的原始值，随后停留在安全空闲状态。

## Video / logs

没有保存视频；完整烧录和 UART 输出位于本会话工具日志中。三段镜像均报告
`Hash of data verified` 并硬复位。

## Result

- ESP-IDF v5.5.5 构建通过，镜像 `niulai.bin` 为 `0x3b830`（243760 bytes），
  应用分区余量 77%。
- 启动日志确认配置为 `SDA=11`、`SCL=12`、400 kHz，且语音/I²S/loopback profile
  均未启用。
- `0x68` 地址 ACK，`WHO_AM_I` 返回 `0x70`；`0x69` 不需要继续作为候选地址。
- 14 字节原始数据读取成功：

  ```text
  MPU6500_RAW address=0x68 accel=-13276,6536,6028 temp=3312 gyro=-529,220,-96
  ```

- 最终结果：

  ```text
  MPU6500_PROBE_RESULT found=1 address=0x68 who_am_i=0x70 id_ok=1 raw_read_ok=1
  MPU6500_PROBE_PASS
  ```

- 结论：**MPU6500 I²C 可达性通过。** 这证明当前地址、SDA/SCL 和基本寄存器读取
  链路正常；原始数值未结合静止姿态、量程或零偏做物理解释。

## Next step

后续若需要使用 IMU，再单独进行静止重力轴、零偏和采样稳定性记录；在此之前不把
MPU6500 接入 FSM、姿态闭环或执行器保护逻辑。后续所有无关测试继续保持麦克风路径
关闭。
