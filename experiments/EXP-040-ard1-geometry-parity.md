# EXP-040 ARD-1 几何与时序对齐验证

## Objective

验证 ESP32 正式运行时是否逐项采用电脑端 ARD-1 的四腿几何：公共
`FL/FR/RL/RR` 映射、核心 `RF/LF/RR/LR` 顺序、IK 膝分支、q→us 符号和静息
脉宽；同时把本轮测试时序固定为 `lift=300 ms`、`swing=360 ms`、
`search=120 ms`。

## Hypothesis

后腿小腿收起角度错误来自 ESP32 将 `RL/RR` 使用了 `BACKWARD` IK 分支，且
PCA9685 适配器用相同的错误后腿静息角作基准；电脑端 ARD-1 四腿均使用
`FORWARD` 分支。若统一分支和适配器基准，软件生成的后腿角度与电脑端应一致。

## Hardware version

目标硬件为 ESP32-S3 rev v0.2、PCA9685 carrier、MPU6500、四腿执行器和原生
Windows `COM4`。Operator 已确认机械固定、人员净空、6 V 电源/电流边界及急停
条件；本轮使用原生 Windows PowerShell 的 CH343 `COM4`。测试脚本仅发送中性保持
包和限定的单腿/双腿掩码，不发送 `HOME`。

## Software commit

分支 `fw/esp32-layered-runtime`，当前工作树未提交。源码路径为
`firmware-esp32/firmware-esp32`。使用 ESP-IDF v5.5.5，目标
ESP32-S3。

## Parameters

- ESP32 formal ARD-1：`lift=300 ms`、`swing=360 ms`、`search=120 ms`。
- 公共腿到载板通道：`FL→CH2/3`、`FR→CH0/1`、`RL→CH6/7`、`RR→CH4/5`。
- 电脑端核心顺序：`RF→CH0/1`、`LF→CH2/3`、`RR→CH4/5`、`LR→CH6/7`。
- 静息脉宽：`1494,1165,1672,1748,1368,1279,1764,1934 us`。
- q→us 斜率：核心顺序髋 `+,-,+,- 836.5 us/rad`，膝
  `-,+,-,+ 624.5 us/rad`；适配器公共顺序使用对应镜像符号。

## Test method

1. 只读比较 `servo_zero_main.c`、`motion_strategy_ard1.c`、`actuator_pca9685.c`
   和 `ard1_core`；确认共享核心 C 源码、头文件和腿数学源码一致。
2. 运行 host suite 与 `tools/run_offline_checks.sh`。
3. 在独立 `build-ard1-geometry-current` 中执行 ESP-IDF v5.5.5 ESP32-S3
   `set-target`/`build`，检查 `config/sdkconfig.h`、`CMakeCache.txt` 和
   `flasher_args.json`。
4. COM4 恢复枚举后，严格按生成 metadata 烧录；启动只读监听确认
   `stage`、IMU ready、`OE=HIGH` 和无 reboot/stack overflow/PCA init error。
5. 在同一安全条件下做四条腿轮换单腿及双腿掩码：单腿 `0x01/0x02/0x04/0x08`，
   双腿 `0x09/0x06/0x03/0x0C`；不发送 `HOME`。

## Video / logs

软件/构建/端口摘录见
[`EXP-040 selected log`](logs/EXP-040-2026-09-17-ard1-geometry-current.txt)。
本轮无实体动作视频。

## Result

软件事实：

- 电脑端 `servo_zero_main.c` 的四条腿均为 `SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD`。
  原 ESP32 策略为前两腿正向、后两腿反向；现已统一为四腿正向。
- PCA9685 适配器原先对公共 `RL/RR` 使用负 IK 静息角；现已与电脑端统一为
  正向 `acosf(cosine)`，后腿镜像只由 q→us 脉宽符号表达。
- 通道映射、静息脉宽和髋/膝斜率与电脑端逐项一致；`ard1_core` 的腿数学和
  四腿核心 C 源码与电脑端副本逐字一致。
- host suite、全量 offline checks 和 `git diff --check` 通过。
- fresh `niulai.bin` SHA-256 为
  `cdbc7077f9f2a70885fa8a88a030e0db104dec64d2d97e8572ebb45e81bda179`；构建
  metadata 为 `0x0` bootloader、`0x8000` partition table、`0x10000` app，
  且生成配置确认 `esp32s3` 与 `300/360/120`。
- 独立 Luna 只读验收复核了同一组五项，并确认 `ard1_core` 的两个源文件与
  电脑端核心副本逐字一致、Ninja 实际编译并链接 `libard1_core.a`。其指出的
  边界是电脑端自定义 NVS 脉宽会改变动态基准；标准 ARD1 静息值下无差异。

硬件事实：

- 烧录前 Windows 原生 `esptool.py v4.12.0 chip_id` 确认 ESP32-S3 rev v0.2，
  MAC `AA:BB:CC:DD:EE:FF`。
- 按 `flasher_args.json` 使用 `0x0/0x8000/0x10000` 烧录，bootloader、partition
  table、application 三段均返回 `Hash of data verified`，随后硬复位。
- 启动串口确认 `ESP-IDF v5.5.5`、`stage=ACTUATION_ENABLED`、
  `runtime_imu: MPU6500 telemetry ready address=0x68`；应用启动后保持
  `armed=0`，`/OE HIGH` 由 boot-first 安全不变量保证。
- 清理 Windows 旧 BLE bond 后重新绑定成功：`encrypted_bonded=1`，STATUS/telemetry
  订阅成功。修正 COM4 监听器先禁用 DTR/RTS 后，共 3 轮完成四单腿
  `0x01/0x02/0x04/0x08` 和四双腿 `0x09/0x06/0x03/0x0C`；每个掩码 8 个动作包
  后 8 个中性包，所有路径到达 `after_actuator_applied`，control 最低
  high-water 为 `3980 B`。
- 动作测试中未见 `reboot`、`stack overflow`、GAP 断开或 PCA9685 初始化/动作帧
  错误；另有独立 MPU6500 遥测 `write_read len=15 ESP_ERR_TIMEOUT`，不等价于
  PCA9685 失败。测试结束 watchdog 输出 `failsafe=1`、`SAFETY_GATE blocked`、
  `FSM_RESET`，随后只读状态回到 `armed=0 failsafe=0`。
- 通过 UART 只读 `status` 再确认 `stage=ACTUATION_ENABLED fsm=SUPPORT
  acceptance=0/100 armed=0 failsafe=0`。本轮串口未输出字面量 `OE=HIGH`，
  但启动代码在任何硬件初始化前先置高，且未 armed 状态维持该安全条件。
- 本轮 UART 证据证明 BLE→控制→FSM→PCA9685 执行器写入链路稳定；它仍不能
  单独证明后腿实际角度/幅度，Operator 需要结合视频、逐帧脉宽和机械观察确认。

## Next step

若实体后腿角度仍偏，下一步应记录实际脉宽/角度/视频和电流，不再从 UART
成功回显推断几何正确；Android 端仍需用新 bond 独立复测 `133/write failed`。

## Follow-up revalidation (2026-09-17)

- 在同一当前工作树上重新确认 `ard1_core/servo_zero_quad_demo.c` 与电脑端副本、
  `ard1_core/servo_zero_leg_math.c` 与电脑端副本逐字相同；共享几何源文件的
  SHA-256 分别为 `dbf81d158dec45b6a72e14f64507d9461a8e6fdccef2bd13ed817292722c36bd`
  和 `96b73ba38ce4befa28fdfea5f30c5c29c3b64a119c5c91e574cd744fd8689908`。
- 重新核对电脑端默认 profile `30,9,40,300,360,120,50`、ESP32 构建配置
  `300/360/120`、40 mm 抬升、30 mm 步长和固定静息脉宽；host ARD-1、quad
  geometry、servo math、全量 host suite 与 41 项 Python 工具测试均通过。
- 普通 WSL shell 的 interop socket 曾无法直接启动 Windows PE；按历史记录改用
  Windows PowerShell 绝对路径并在主机权限上下文恢复访问，确认 `USB-Enhanced-SERIAL
  CH343 (COM4)` 状态为 `OK`、Windows Python 3.14 和 esptool 4.12.0 可用。
- 本次重新烧录并完成实际 BLE→执行器路径验证；但实体后腿舵机角度、逐帧脉宽和视频
  对照仍未由串口工具自动测得，不能把软件路径成功升级为机械幅度完全等价。
