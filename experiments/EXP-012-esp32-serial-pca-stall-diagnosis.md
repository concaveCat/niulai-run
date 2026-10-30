# EXP-012 ESP32-S3 串口到 PCA9685 卡顿分层排查

## Status

Result：软件根因已确认并修复；实体舵机动作验收仍待 Operator 在满足安全条件的台架完成。

## Objective

区分 Host/USB/Serial、ESP32 parser/control task 和 PCA9685 I²C 写入是否造成报告中的
数百毫秒“停—跳—停”现象。测试保持单变量流程；所有本次硬件测试均为 `/OE HIGH` dry-run，
不授权舵机输出。

## Test setup and safety

- ESP32-S3 rev 0.2，CH340 `1a86:55d3`，WSL `/dev/ttyACM0`（USB/IP 路径）。
- PCA9685 地址 `0x40`，SDA=`GPIO11`，SCL=`GPIO12`，PWM=`50 Hz`，整帧为 8 路
  auto-increment 写入；ESP-IDF `v5.5.5`，工具链路径见 `debug_log.md`。
- Host 使用 `time.perf_counter_ns()`；ESP32 使用 `esp_timer_get_time()`；脉宽只在软件范围
  `1500 ↔ 1505 us` 间变化。`/OE` 在烧录、启动和全部 dry-run 期间保持 HIGH。
- 软件 commit：未提交工作区（`fw/kw10-perfect-touchdown`）；原始日志保存在
  `/tmp/niulai-servo-trace-rx-20260912-v2.log`、
  `/tmp/niulai-servo-trace-local-20260912.log`、
  `/tmp/niulai-servo-trace-e2e-dry-20260912.log`、
  `/tmp/niulai-servo-trace-legacy-500ms-20260912.log`、
  `/tmp/niulai-servo-trace-legacy-20ms-20260912.log`、
  `/tmp/niulai-servo-trace-legacy-20ms-30s-20260912.log` 和
  `/tmp/niulai-servo-trace-e2e-fixed-20260912.log`。

## Method

诊断代码在固定大小 ring buffer 中记录 `ESP_RX`、`CTRL_ACCEPT`、`CONTROL_TICK`、
`I2C_BEGIN` 和 `I2C_END`。Host 发送循环不等待回复、不逐条 flush；实验结束后分 32 条分页
读取，避免诊断输出自身阻塞任务。ESP32 parser 改为每轮只读取当前可用字节，遇到换行才解析。

## Results

### Experiment 2: RX-only

500 条、20 ms 周期，PCA9685 输出路径关闭：

- Host TX：`min=17.650 ms`，`mean=20.000 ms`，`p95=20.128 ms`，`p99=20.188 ms`，
  `max=22.293 ms`。
- ESP RX：500 条，`mean=20.003 ms`，`p95=22.084 ms`，`p99=27.394 ms`，`max=45.129 ms`；
  <=2 ms batch 为 2，>60 ms gap 为 0，序号 `0..499` 完整，`dropped=0`。

结论：本次 WSL/USB-IP RX-only 路径没有出现“数百毫秒沉默后批量到达”，不能作为本次卡顿
根因。

### Experiment 1 and E2E baseline

ESP32 内部 `pose pseudo` dry-run 的 3.5 s 记录有 127 个约 20 ms control tick；控制/更新
间隔均值 `19.842 ms`、p95/p99 `20.000 ms`、最大 `20.013 ms`。I²C 写入均值 `3.196 ms`、
最大 `3.276 ms`，无记录丢失。

标准 latest-target E2E dry-run（500 条）中，ESP RX 均值 `19.980 ms`、最大 `29.862 ms`，
>60 ms gap 为 0；I²C 均值 `3.206 ms`、最大 `3.249 ms`，500 条 `I2C_END` 均为 `result=0x0`。

### Single-variable A/B: old refresh writer

A：只恢复旧 `CONFIG_SERVO_ZERO_REFRESH_MS=500`，使用 `trace run-legacy-dry`，Host 仍以
20 ms 发送 500 条。Host TX 均值 `20.000 ms`，ESP RX 均值 `19.980 ms`、最大 `40.008 ms`；
I²C duration 均值 `3.212 ms`、20/20 次成功，但 `I2C_BEGIN` 只有 20 次，间隔
`min=499.986 ms`、`mean=499.999 ms`、`max=500.000 ms`。

B：唯一行为变量改为 `CONFIG_SERVO_ZERO_REFRESH_MS=20`，同样的 500 条 dry-run。I²C
`495/495` 次成功，间隔 `min=19.974 ms`、`mean=20.202 ms`、`max=40.000 ms`，无 >60 ms
间隔；duration 均值 `3.203 ms`、最大 `3.247 ms`。

长时 B：1500 条、连续 30 s、20 ms dry-run：ESP RX `1500/1500`，序号 `0..1499`，
`dropped=0`，RX `mean=19.994 ms`、p95 `29.595 ms`、p99 `29.790 ms`、max `39.870 ms`，
<=2 ms batch 为 2，>60 ms gap 为 0；I²C `1497` 次全成功，`I2C_BEGIN` 间隔
`min=19.975 ms`、`mean=20.040 ms`、`max=40.000 ms`，duration `mean=3.288 ms`、
max `3.416 ms`。

## Root cause and fix

根因是隔离舵机路径的 `servo_refresh_task` 使用 `CONFIG_SERVO_ZERO_REFRESH_MS=500`。
Host 命令和 ESP parser 实际约 20 ms 到达且被接受，但 `s_pulse_dirty` 只在该任务下次
醒来时提交整帧，因此新的脉宽最多每 500 ms 才进入 PCA9685，形成数百毫秒停顿后跳变。
I²C 本身每次约 3.2 ms、无 NACK/timeout；本次证据不支持 parser、WSL batching 或 I²C
retry 是这次现象的根因。

修复将刷新周期默认值和合法下限改为 20 ms。实时目标仍采用 latest-value-wins；离散动作
路径未改为丢弃事件。非阻塞 parser、trace ring buffer、有限的 I²C 单次传输和 20 ms
独立 writer 保留用于回归诊断。

## Verification and remaining work

- Host Python tests：8 tests passed；`git diff --check` passed。
- ESP-IDF v5.5.5 isolated build passed；20 ms 镜像三段均 `Hash of data verified` 并硬复位。
- 500 条标准 latest-target E2E dry-run：ESP RX 500 条，batch=0，>60 ms gap=0；I²C
  500/500 成功，duration mean `3.204 ms`、max `3.247 ms`。
- 最终镜像第一次烧录遇到 USB/IP 瞬态 `C100: Bad data checksum`，当时 sysfs 可见但
  `/dev/ttyACM0` 暂时消失；`udevadm settle` 后按同一构建元数据重试，三段镜像均
  `Hash of data verified`。最终镜像 100 条 smoke trace 的 I²C interval 为
  `19.975..20.000 ms`、duration mean `3.169 ms`，全部成功。
- 已完成 30 s `/OE HIGH` 软件/总线长测；未运行实体舵机动作，因此不能声称已经完成机械
  “不卡顿”、限位、电流、压降、噪声或温升验收。Operator 需在机械固定、单舵机、小幅度、
  限流、人员净空和急停条件齐全后做最终低风险 A/B。

## Rollback

恢复 `CONFIG_SERVO_ZERO_REFRESH_MS` 的旧配置会重现已量化的约 500 ms writer 空洞，不应作为
实时目标模式的默认值；如需复现基线，仅使用 `/OE HIGH` 的 `trace run-legacy-dry`。
