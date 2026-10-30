# EXP-039 BLE 控制链路与 control 栈修复验证

## Objective

验证当前 layered-runtime 固件在 Windows 原生 BLE Central 下，开启 STATUS/IMU
通知时，能否连续接收四条单腿和双腿输入并进入执行器应用路径；同时定位此前
Windows Bleak `Unreachable`/手机 `133` 的连接阻塞原因。

## Hypothesis

旧 Windows BLE bond 可能与 ESP32 的加密 bonding 状态不一致；control task 中的
通知调用和过小的 4096 B 栈可能使动作路径不稳定。清除旧 bond、将 control 栈
提高到 8192 B，并把通知移到独立的 100 ms TX task 后，连接和动作路径应保持稳定。

## Hardware version

ESP32-S3 revision v0.2，USB-Enhanced-SERIAL CH343 原生 Windows `COM4`，BLE
地址 `AA:BB:CC:DD:EE:FF`；共享 I²C 上的 PCA9685、MPU6500 和 KW10。Operator
已确认机械固定、人员净空、6 V 电源/电流边界和急停条件。动作测试使用已授权的
诊断单腿/双腿输入掩码；未发送 `HOME`。

## Software commit

分支 `fw/esp32-layered-runtime`，当前工作树未提交。源码路径为
`firmware-esp32/firmware-esp32`。使用 ESP-IDF v5.5.5、目标
ESP32-S3；构建目录为
`firmware-esp32/build-ard1-fixed-current`。改动包括 control 栈 8192 B、独立
BLE TX task、GAP 安全状态日志和本诊断工具；Windows bond 清理工具为
`tools/reset_windows_ble_bond.py`。

## Parameters

- Flash 参数严格来自 `build-ard1-fixed-current/flasher_args.json`：
  `0x0` bootloader、`0x8000` partition table、`0x10000` application。
- 应用 `niulai.bin` SHA-256：
  `9feac4f497ae0234552e3eb8935cab07a8a052612c4379ec9a1e89da2dfa158e`。
- BLE test：先发送 125 个 100 ms 中立包满足 10 s gate；3 轮，每轮单腿
  `0x01/0x02/0x04/0x08`（FL/FR/RL/RR）和双腿 `0x09/0x06/0x03/0x0C`，
  每组 8 个动作包 + 8 个中立包；STATUS 与 telemetry 均订阅。
- UART 115200；仅监听运行时日志。测试结束停止发包，由 500 ms watchdog
  进入 failsafe。

## Test method

1. 观察旧 bond 连接：ESP32 报 `encrypted_bonded=0`，随后断开。
2. Windows WinRT 对牛来快跑执行 `unpair_async()`，返回 `UNPAIR_STATUS=0`。
3. 使用 fresh image 按 `flasher_args.json` 烧录 COM4；三段均校验通过并硬复位。
4. 用 `BleakClient(pair=True)` 重新配对，订阅 STATUS/telemetry，执行上述 3 轮
   掩码序列，串口过滤记录 GAP、FSM、安全门、PCA/I²C 和栈诊断。

## Video / logs

无视频。串口和 BLE 主机输出的关键摘录保存在
[`EXP-039 selected log`](logs/EXP-039-2026-09-16-ble-control-stack-repair.txt)。
完整终端输出未作为二进制原始附件保存；本记录不把摘要当作机械测量。

## Result

事实观测：

- 旧 bond 失败为 `BLE security ... encrypted_bonded=0`，随后
  `GAP disconnect ... reason=531`；Windows 主机表现为首个写入
  `Unreachable`。清理 bond 后重新配对成功：`GAP connect status=0`、
  `encrypted_bonded=1`。
- `BLE_NOTIFICATIONS=subscribed` 成功；3 轮全部完成，所有 4 个单腿掩码和
  4 个双腿掩码均发送并走到 `after_actuator_applied`。没有 `Unreachable`、
  GAP 断开、`stack overflow` 或 `reboot`。
- control 最低 high-water 为 `3996 B`（8192 B 栈）；结束后出现预期的
  `WATCHDOG failsafe=1`、`SAFETY_GATE blocked reason=failsafe` 和
  `FSM_RESET state=SUPPORT`。
- 烧录 bootloader、application、partition table 三段均报告
  `Hash of data verified`。启动确认 `stage=ACTUATION_ENABLED` 和
  `runtime_imu: MPU6500 telemetry ready address=0x68`；未见 PCA9685 初始化
  异常。
- 另有独立遗留异常：MPU6500 telemetry 的 `write_read len=15` 出现
  `ESP_ERR_TIMEOUT(0x107)`。本轮没有把它归因于 PCA9685，也没有用 UART
  摘要宣称实体腿的方向、位移、电流、温升或机械稳定性已经验收。

解释：当前“被阻塞”的直接原因是 Windows 旧 BLE bond 与 ESP32 端安全状态
不一致；control 栈/通知耦合是固件中已修复并经本轮压力路径验证的稳定性风险，
不是这次 Windows `Unreachable` 的直接原因。

## Next step

保留本镜像作为软件基线；另行修复/隔离 MPU6500 telemetry I²C timeout，并由
Operator 进行带视频/电流/温升记录的低幅实体动作验收。Android 手机仍需在新
bond 上独立复测 `write failed: 133` 是否消失；不得将本次 Windows Central
结果直接等同于 Android GATT 验收。
