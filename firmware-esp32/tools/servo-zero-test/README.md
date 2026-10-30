# 隔离 PCA9685 舵机测试固件

这是一个与正式 BLE、FSM、显示、音频和执行器 adapter **完全隔离**的 ESP-IDF 测试工程。它只做一件事：通过 UART 手动控制 PCA9685 的 CH0–CH7，用于测量零位、方向、脉宽范围和机械限位。

> 这里的 `300..3000 µs`、`500..2500 µs` 等是**软件传输/测量范围**，不是舵机或连杆的机械安全范围。`/OE` 是动作禁止，不是断电。实体动作前必须机械固定、限流、人员净空，并能立即断开 6 V / XT30。

---

## 1. 硬件与通道

- PCA9685：I²C `0x40`，50 Hz，CH0–CH7 输出，CH8–CH15 `FULL_OFF`；
- `/OE`：GPIO1，启动即高；只有显式 `run` / `step` / `pose` / `demo` 才拉低；
- 默认 I²C 400 kHz；每次写入最多尝试 4 次，失败间隔约 1 ms；连续失败才拉高 `/OE`；
- 静态输出只在脉宽改变时重写整帧；动作任务 20 ms 更新。

固定通道映射：

```text
CH0 rf_hip   右前髋       CH1 rf_knee  右前膝
CH2 lf_hip   左前髋       CH3 lf_knee  左前膝
CH4 rr_hip   右后髋       CH5 rr_knee  右后膝
CH6 lr_hip   左后髋       CH7 lr_knee  左后膝
CH8..CH15    FULL_OFF / 未使用
```

当前样机确认的直立中立候选：

```text
CH0..CH7 = 1746, 1469, 1427, 1415, 1619, 1582, 1449, 1600 us
```

这是**特定样机的标定候选**，不是通用安全值。

---

## 2. 构建与烧录

使用独立 build 目录，避免覆盖正式固件：

```bash
cd firmware-esp32/tools/servo-zero-test
idf.py -B build-servo-zero set-target esp32s3
idf.py -B build-servo-zero build
idf.py -B build-servo-zero -p /dev/ttyACM0 flash monitor
```

Windows 使用 `COMx`。需要修改脉宽、I²C 地址或 Gerber 候选 GPIO 时：

```bash
idf.py -B build-servo-zero menuconfig
# 牛来快跑 isolated CH0 servo-zero test
```

---

## 3. UART 命令

```text
help
select 3                  # 选择通道
pulse 1515                # 设置脉宽（us）
ms 1.515                  # 设置脉宽（ms）
angle 3                   # 名义角度换算，1000..2000 us = 180°
list                      # 查看当前八路
center                    # 八路回 1500 us（内存）
all 1500                  # 八路同时设置
save / load               # NVS 保存 / 丢弃未保存修改
stop / run                # /OE 高 / 低
frame 1741,1464,1432,1420,1614,1577,1454,1605
stats reset / stats
```

### 一次性单腿动作

```text
step rf [PCT]             # 右前腿抬起/放下，PCT=0..100
step rf-ik DEG            # 按 EXP-010 公式的单腿动作
pose rest [PCT] [CH...]   # 静息姿态探针
pose pseudo [PCT]         # 对角错峰伪同步静息
pose pseudo-split [PCT]   # 半帧 I²C A/B
```

### demo

```text
demo ch1-cosine
demo rf-hip-vertical POS_US NEG_US
demo rf-lift-cosine-drop [DEG]
demo knee-swing [PCT]
demo quad 1..4
demo quad press MASK [PROFILE]
demo quad release MASK
rest direct
```

`demo quad` 的键位：

```text
1 = RF / 右前 = CH0, CH1
2 = LF / 左前 = CH2, CH3
3 = RR / 右后 = CH4, CH5
4 = LR / 左后 = CH6, CH7
```

掩码位：`RF=1`、`LF=2`、`RR=4`、`LR=8`。最多两条腿同时摆动。

---

## 4. 上电检查清单

1. **先拆下全部舵机臂**，逐一核对 CH0–CH7 的 GND/V+/PWM 方向；不要接未确认针脚顺序的舵机。
2. 固定所有舵机和板，限制意外转动范围；人员、线缆、工具退出运动范围。
3. 在线束电源端安装规定的 **10 A、≥32 VDC 保险**；使用受控 6 V 电源并设置合理限流。
4. 确认可以立即断开 6 V / XT30；`/OE` 不是断电开关。
5. 第一次上电不要装腿部连杆，观察 UART 出现 `CH0-CH7 READY`。
6. 从单通道、低幅度、短时开始；记录方向、限位、电流、压降和温升。
7. 出现异响、撞限位、过流、掉压、发热或异常动作，立即 `stop` 并断开 6 V。

---

## 5. 高风险命令

以下命令会直接驱动舵机，属于 **HIGH RISK**：

- `step rf` / `step rf-ik`；
- `pose rest` / `pose pseudo` / `pose pseudo-split`；
- `demo ch1-cosine` / `demo rf-hip-vertical` / `demo rf-lift-cosine-drop` / `demo knee-swing` / `demo quad`；
- `run`、`frame`、`all`、`pulse`。

要求：

- 从最低幅度开始；
- 机械固定、限位、6 V 限流、保险、人员净空；
- 随时可 `stop` 并断开 6 V；
- 不把串口无 NACK 当成实体验收；
- 不把 `300..3000 µs` 当成机械安全范围。

多舵机同时动作时，I²C NACK 可能来自 6 V 瞬态、公共地、线束、机械阻力或电源能力；软件重试不能替代电源修复。相关复盘见 [`../../../docs/NACK_POWER_DIAGNOSIS_CASE_STUDY.md`](../../../docs/NACK_POWER_DIAGNOSIS_CASE_STUDY.md)。

---

## 6. Wi‑Fi TCP 诊断（可选）

USB 不可用时，可开启同一工程的临时 SoftAP/TCP 诊断：

```text
SSID: 牛来快跑-DIAG
密码: 牛来快跑-Diag
地址: 192.168.4.1:3333
```

主机：

```bash
python3 ../../../tools/servo_zero_tcp_console.py --host 192.168.4.1
```

该接口无认证、日志/命令队列有界，只允许本地台架使用；不能暴露到其他网络。详见 [`../../../docs/SERVO_ZERO_TCP_DIAGNOSTIC.md`](../../../docs/SERVO_ZERO_TCP_DIAGNOSTIC.md)。

---

## 7. Host 数学检查

```bash
cc -std=c11 -Wall -Wextra -Werror \
  main/servo_zero_math.c main/servo_zero_leg_math.c tests/test_servo_zero_math.c -lm \
  -o /tmp/niulai-servo-zero-math-test
/tmp/niulai-servo-zero-math-test
```

四腿足端空间 demo：

```bash
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main \
  main/servo_zero_leg_math.c main/servo_zero_quad_demo.c \
  tests/test_servo_zero_quad_demo.c -lm \
  -o /tmp/niulai-servo-zero-quad-demo-test
/tmp/niulai-servo-zero-quad-demo-test
```

`servo_zero_us_profile.[ch]` 是纯 µs 空间的触地/搜索数学层，不接入正式执行器；相关实验见 [`../../../experiments/EXP-023-us-space-touchdown-search.md`](../../../experiments/EXP-023-us-space-touchdown-search.md)。

---

## 8. 边界

- 不接入正式 BLE、FSM、HUD、音频或安全门；
- 不自动运行任何动作；
- 不把示例 profile 当成实体标定；
- 不把 host 数学测试当成机械/供电验收；
- 任何正式动作仍必须经过 [`../../README.md`](../../README.md) 的安全门和台架流程。
