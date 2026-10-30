# EDD-005 MASTER_ENABLE / ESTOP hardware design v0.1

状态：**Topology selected / exact logic MPN pending**。本文件是 Circuit Design 草稿，不是 schematic 或 Circuit Design Freeze。

## 1. 选择的安全逻辑

使用 AON 3.3 V 域中的硬件 fault latch。逻辑状态定义：

- `MASTER_ON=1`：实体 MASTER 开关处于 ON；
- `ESTOP_LOOP_OK=1`：外部常闭回路闭合；
- `SERVO_FAULT_LATCH=1`：已发生 ESTOP/open-loop fault；
- `SERVO_HW_ENABLE=MASTER_ON AND NOT SERVO_FAULT_LATCH`。

`SERVO_HW_ENABLE` 只进入板载 SY8388 EN、USB servo-source enable 和其后的 source-isolation enable。它不由 ESP32 或 CH32 产生。

## 2. Fault latch implementation

选择带异步 preset/clear 的单 D flip-flop 作为状态存储：

- `Q=SERVO_FAULT_LATCH`；
- asynchronous preset 在 `ESTOP_LOOP_OK=0` 或 AON power-on-reset 时有效，使 Q=1；
- asynchronous clear 只在 `MASTER_ON=0 AND ESTOP_LOOP_OK=1` 时有效，使 Q=0；
- MASTER 再次 ON 后 clear 释放，Q 保持 0；
- ESTOP 重新闭合但 MASTER 仍 ON 时，preset 释放而 clear 不成立，Q 继续保持 1；
- D/CLK 不用于正常状态改变，固定到确定逻辑电平。

必须选择上电/掉电行为有可靠规格、支持 3.3 V AON 域的 exact flip-flop/gates。若异步 preset/clear 同时有效可能进入非法状态，则外围组合逻辑必须保证 ESTOP open 时 preset 优先、clear inactive。

## 3. External NC input

接口定义：

```text
J_ESTOP pin 1 = AON_3V3 through protected/limited source
J_ESTOP pin 2 = ESTOP_SENSE_RAW

ESTOP_SENSE_RAW -- 1 kΩ -- ESTOP_SENSE
ESTOP_SENSE -- 10 kΩ --> GND
ESTOP_SENSE -- 100 nF --> GND
ESTOP_SENSE --> Schmitt input --> ESTOP_LOOP_OK
```

回路闭合时 `ESTOP_LOOP_OK=1`；按下、拔出或断线时 10 kΩ 下拉产生 `ESTOP_LOOP_OK=0`。connector 侧需低电容/低漏电 ESD，exact MPN 可与足端慢速输入保护共料，但必须按 AON 电压和外部线缆浪涌重新确认。

[`verify_estop_frontend.py`](../calculations/verify_estop_frontend.py) 使用 R±1%、C±10%、CMOS guaranteed 0.3/0.7 VDD 阈值屏选：

- loop open 到 guaranteed-low：约 1.073–1.338 ms；
- loop close 到 guaranteed-high：≤0.148 ms；
- open detection 给 latch/gate 留下超过 8 ms 的余量，满足 ≤10 ms servo-source shutdown。

## 4. Truth table

| MASTER_ON | ESTOP loop | Prior fault | Fault latch next | Servo power |
| ---: | ---: | ---: | ---: | ---: |
| 0 | open | X | 1 (set priority) | OFF |
| 0 | closed | X | 0 (clear) | OFF |
| 1 | open | X | 1 | OFF within 10 ms |
| 1 | closed | 1 | 1 | OFF; closing ESTOP alone cannot recover |
| 1 | closed | 0 | 0 | eligible; other source/arm conditions still apply |

恢复序列固定为：`ESTOP closed → MASTER OFF for ≥1 ms → MASTER ON`。MASTER OFF 的 ≥1 ms 同时覆盖 INA300 latch 的 ≥20 µs clear requirement；若过流仍存在，重新 ON 后会再次立即关断。

## 5. Default and fault behavior

| Event | Required hardware result |
| --- | --- |
| AON rail rising, MASTER already ON | POR presets fault latch; servo remains OFF until a valid MASTER OFF→ON cycle |
| ESP32/CH32 reset or unprogrammed | no effect on ESTOP ability; PWM buffer remains separately disabled by its pull-up/OE chain |
| ESTOP cable open/removed | latch set and all servo sources disabled within 10 ms |
| ESTOP cable reconnected | latch remains set |
| MASTER control wire open | passive default produces MASTER_OFF and servo OFF |
| AON rail brownout | gate pull-downs keep SY8388/source switches disabled; POR restores fault-latched state after recovery |

## 6. Remaining exact selections

The topology is closed, but the following exact MPNs remain an ordinary component-selection task:

- AON-compatible D flip-flop with asynchronous preset/clear;
- Schmitt input and minimal OR/AND gates, preferably single/dual-gate packages;
- POR implementation or supervisor;
- ESD part for the external ESTOP cable;
- exact 2P locking ESTOP connector.

No MCU, CPLD or firmware is part of the safety execution path.
