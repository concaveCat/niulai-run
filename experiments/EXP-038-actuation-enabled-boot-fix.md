# EXP-038 ACTUATION_ENABLED 映射修复与启动检查

## Objective

验证免 PIN 固定台架 profile 的 ACTUATION_ENABLED 镜像能够启动并完成
PCA9685 安全初始化，同时保持运行时安全门关闭；记录首版镜像暴露的
兼容单腿通道映射故障及修复结果。

## Hypothesis

正式四腿物理通道顺序为 `FR→CH0/1、FL→CH2/3、RR→CH4/5、RL→CH6/7`。
若兼容单腿路径沿用同一映射，默认 `ACTIVE_LEG_FL` 的 `1200 us` lift 目标
不会被错误套用到 CH0 的 `1400 us` 下限，PCA9685 初始化应通过。

## Hardware version

ESP32-S3 (revision v0.2) + USB-Enhanced-SERIAL CH343 (COM4)；固件使用
共享 I²C 上的 PCA9685、MPU6500 和 KW10 输入。Operator 已确认本次台架的
机械固定、限位、供电/电流边界、人员净空和急停条件；本实验没有发送 BLE
控制包或实体运动命令。

## Software commit

分支 `fw/esp32-layered-runtime`，当前工作树未提交。修复涉及
`firmware-esp32/components/actuator_pca9685/include/pose_map.h`、
`firmware-esp32/components/actuator_pca9685/actuator_pca9685.c` 及 host
映射回归。构建使用 ESP-IDF v5.5.5、ESP32-S3 和外部 `SDKCONFIG`，确认
`CONFIG_NIULAI_MODE_ACTUATION_ENABLED=y`、`CONFIG_NIULAI_ACTIVE_LEG_FL=y`。

## Parameters

- PCA9685: SDA GPIO11、SCL GPIO12、地址 `0x40`、50 Hz、`/OE` GPIO1。
- Formal rest pulses: `1494,1165,1672,1748,1368,1279,1764,1934 us`
  (CH0..CH7)。
- Reviewed hard envelope: CH0 minimum 1400 us、CH2 maximum 1750 us、CH4
  maximum 1940 us、CH6 minimum 1120 us；其它通道 500..2500 us。
- Runtime acceptance: 100 watchdog ticks / 10 s；watchdog timeout 500 ms。
- UART: 115200 baud；仅发送无运动副作用的 `status` 查询。

## Test method

1. 将首版 ACTUATION_ENABLED image 烧录 COM4 并监听启动。
2. 记录首版 PCA9685 初始化结果。
3. 将兼容单腿映射与正式载板通道顺序统一，并补充 FL→CH2/CH3 host 回归。
4. 重新运行 host suite，重新构建并按生成的 `flasher_args.json` 烧录三段镜像。
5. 监听 8 秒启动日志，再发送一次 `status`，确认阶段、FSM、安全门和
   failsafe 状态；不执行配对、HOME 或运动。

## Video / logs

原始摘要和命令结果见
[`EXP-038 raw log`](logs/EXP-038-2026-09-16-actuation-boot-fix.txt)。没有视频。

## Result

首版镜像三段写入均通过 `Hash of data verified`，但启动输出
`pose pulse configuration outside safe range`，随后
`SAFETY_GATE pca9685_init_failed staying_on_null_actuator`。根因是兼容
单腿映射将 `ACTIVE_LEG_FL` 解析为 CH0，而 `lift_a=1200 us` 超过 CH0 的
已审阅下限；不是 PCA9685 总线或烧录偏移错误。

修复后 host suite 全部通过；ESP-IDF 增量构建通过，应用分区余量 27%。
修复版应用 SHA-256 为
`037af1337fd658a7118d83542097e523b942e2048621c874b186fa8f85cc2325`。
修复版 bootloader、application、partition table 均重新烧录并报告
`Hash of data verified`。启动监听确认：

- `stage=ACTUATION_ENABLED active_leg=ALL`；
- GATT 控制/HOME 特征为 `WRITE|WRITE_ENC`；
- `runtime_imu: MPU6500 telemetry ready address=0x68`；
- 没有再次出现 PCA9685 初始化失败、stack overflow 或 reboot 日志；
- UART `status` 返回 `QW_STATUS stage=ACTUATION_ENABLED fsm=SUPPORT acceptance=0/100 armed=0 failsafe=0`。

这证明软件初始化路径和安全门初始状态符合预期，但不证明舵机实际姿态、
方向、限位、电流、温升、免 PIN 手机配对或实体动作。

## Next step

用 Android APK 完成无 PIN 加密 bonding、首个中性控制包和 STATUS/IMU 通知
验收；只有在重新确认急停和实体边界后，按低幅度、单阶段动作计划进行台架
动作，并在任何异常时保持 `armed=0`/`/OE HIGH`。
