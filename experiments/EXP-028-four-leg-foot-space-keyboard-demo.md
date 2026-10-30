# EXP-028 Four-leg Foot-space Keyboard Demo

## Status

Testing — isolated software/demo path with an all-leg rest preamble; not an accepted formal gait
or entity-drive result.

## Objective

验证键盘 `1..4` 是否能显式选择一条腿执行实时按住/松开动作：先由小腿抬高、再由大腿
连续摆动，松键后冻结大腿并只下放小腿；未摆动的支撑腿在 body frame 中按全局分配器后退。

## Scope and mapping

本实验只改动隔离 `servo-zero-test` 路径，不接正式 BLE/FSM、IMU 或 KW10。键位与当前
隔离台架记录显式对应为 `1=RF(CH0/1)`、`2=LF(CH2/3)`、`3=RR(CH4/5)`、`4=LR(CH6/7)`。
ASCII 控制台先发送 `rest direct` 并等待完整四腿静息；固件也要求该静息前置状态，不能把未静息的
`demo quad` 直接当作有效动作。正式执行器仍存在历史腿/通道映射冲突，不能把本实验的枚举
顺序直接用于正式路径。

## Software parameters

- 连杆模型：大腿 `45 mm`，小腿 `70 mm`；
- 起始足端：每腿 `(x,z)=(0,-100) mm`，之后保存最近一次成功命令的足端估计；
- 当前交互 profile：`30,9,40,360,300,120,25`，即主动腿步长请求 `30 mm`、膝抬高 `40 mm`、实时水平速度比例 `25%`；30 mm 抬腿和 50% 速度比例仍可通过显式 profile 对照；三条支撑腿使用全局守恒分配并各自独立裁剪；主动足端受 `±50 mm` 软件工作区夹持；固件预留显式 lift 上限 `50 mm`；
  越过某条腿独立髋止挡的请求只在该腿最后可用足端位置饱和；
- 节拍：`20 ms`；当前交互 profile lift/swing/search：`360/300/120 ms`；`swing_ms` 只用于计算名义摆动速度，不是按住时限；
- 主动腿：膝先 lift → 髋连续 forward swing → 松键后固定髋、膝 timed search；
- 其余三腿：z 保持当前估计值，x 在当前 profile 中后退 `9 mm`；
- 工作空间软限位：足端 `x=-35..+35 mm`、`z=-105..-50 mm`；到达后限位后保持，避免多轮动作因累计后退而触发不可达 IK；
- 髋脉宽限位按通道独立定义：CH0 下限 `1400 us`、CH2 上限 `1750 us`、CH4 上限 `1940 us`、CH6 下限 `1120 us`；它们分别对应前腿向后、前腿向后、后腿向前、后腿向前；CH1/3/5/7 明确没有已知机械止挡，不共享髋限位；
- Demo 前置：四腿 `rest direct` 完成后保持静息，才允许第一次数字键动作；`stop` 后需重新 rest；
- 每个采样点：四腿 IK、每腿独立候选 q→us、足端软限位、每通道独立单向脉宽限位、候选 `300..3000 us` 范围和相邻脉宽跃度预检；软限位越界只夹住对应腿，IK/通用脉宽/跃度/I²C 失败才拒绝或停机。

## Acceptance criteria

1. Host test 对四个键位对应的主动腿均生成有效完整路径。
2. 中间 swing 样本中主动腿 x 向前、其余三腿 x 向后，且支撑腿 z 不变。
3. 每个采样点都输出八路同一快照的脉宽；足端进入软限位后保持，仍须通过 IK、重复通道、脉宽范围和跃度检查。
4. `tools/quad_ascii_walk.py` 启动时先发送 `rest direct`；数字键按下/松开分别发送独立的 `press/release`，不再把数字解释为通道选择。
5. ESP-IDF 构建、离线 host/tool/documentation checks 通过。
6. 连续 32 次模拟数字键动作都能完成规划；支撑腿或主动腿到达各自软限位后保持，不因该限位拒绝整次四腿动作。
7. 膝主导 profile 在 40 mm 抬腿时保持髋小幅连续摆动；RF/LF/RR/LR 及 LF+RR 组合均能通过独立限位和整帧软件预检。

## Evidence

Hardware version: ESP32-S3 QFN56 rev v0.2 with CH343 COM4 and PCA9685; software-only and
hardware trial evidence are separated below.
Software commit: uncommitted shared worktree on `fw/kw10-perfect-touchdown`.
Raw log/video: none; the commands below are the reproducible host/build evidence.

Repeated checks and results:

- `cc -std=c11 -Wall -Wextra -Werror -pedantic -I main main/servo_zero_leg_math.c main/servo_zero_quad_demo.c tests/test_servo_zero_quad_demo.c -lm -o /tmp/niulai-servo-zero-quad-demo-test && /tmp/niulai-servo-zero-quad-demo-test` — pass, four active-leg plans, invalid-path rejection, independent one-sided endpoint checks, start-foot limiting, and 32-step re-center/continue simulation.
- `python3 -m unittest tools.tests.test_servo_zero_console -v` — pass, 7 tests including rest-before-digit ordering and the four keyboard commands.
- `ninja -C firmware-esp32/tools/servo-zero-test/build-servo-zero` — pass with ESP-IDF v5.5.5 toolchain; latest `niulai_servo_zero_test.bin` size `0xd5610`, `0x2a9f0` bytes free.
- `sh tools/run_offline_checks.sh` — pass; Python/tool/host-C/documentation checks and `git diff --check` completed successfully.

- 独立限位 host regression — pass：验证 CH0/CH2/CH4/CH6 各使用自己的单向端点，膝通道不
  复用髋端点；大幅主动腿 `40 mm`（实际夹到 `+35 mm`）/支撑腿 `8 mm` 四个主动腿路径均通过；
  32 步模拟中支撑腿到达自身髋脉宽软限位后保持，后续动作继续完成而没有发送 stop。
- 同一专项 C 可执行文件连续运行 `100` 轮 — pass；每轮均通过四个键位路径、逐通道方向
  边界、未定义方向放行和软限位饱和后继续检查。
- `sh tools/run_offline_checks.sh` — pass；包含 ESP32 host 全套、Python/tool tests、文档
  链接/索引审计和 `git diff --check`。

The initial software-only run did not flash firmware or drive an actuator because the non-RF
q→us maps, formal channel mapping, four-way contact semantics, mechanical limits and
power/estop evidence were not complete. The failed `idf.py --version` probe only showed that
the wrapper was not on `PATH`; the build was completed from the recorded CMake/Ninja metadata.

Attempted hardware run on 2026-09-13: both ordinary and approved read-only hardware-context
probes reported only the WSL2 USB/IP virtual host controllers (`1d6b:0002` and `1d6b:0003`).
There were no `/dev/ttyACM*` or `/dev/ttyUSB*` nodes, no Windows interop tools in this WSL
environment, and the recorded `192.168.4.1:3333` TCP diagnostic endpoint failed a one-second
connectivity probe (`nc -z -w 1 ...`, exit 1). The existing `CMakeCache.txt` and
`flasher_args.json` were inspected; no guessed port or flash offset was used. No flash, serial
command, or actuator movement occurred.

Hardware deployment and one authorized RF trial on 2026-09-13: the current image was rebuilt
with `ninja -C firmware-esp32/tools/servo-zero-test/build-servo-zero`, producing
`niulai_servo_zero_test.bin` size `0xd4c00` with `0x2b400` bytes free. Host PowerShell
esptool v4.12.0 wrote the three files from `flasher_args.json` to COM4 at offsets `0x0`,
`0x10000`, and `0x8000`; all three reported `Hash of data verified`, followed by a hard reset.
The read-only boot probe reported the isolated project, PCA9685 `0x40` ACK, `50 Hz / 400 kHz`,
`CH0-CH7 READY`, `CH8-CH15 FULL_OFF`, and `/OE HIGH`.

With the Operator's current safety confirmation, the host sent `stop`, `demo quad 1`, waited
for the nominal 2350 ms path, and sent `stop` again. UART reported `key=1 leg=RF` accepted,
`quad demo complete`, then `UART stop -> /OE HIGH`. The subsequent `stats/list` query reported
`logical=133 attempts=136 success=133 nack=3 timeout=0 other=0 retries=3 recovered_after_retry=1 terminal_failures=0`;
the final listed targets were `CH0..CH7 = 1951,1515,1552,1369,1494,1628,1574,1554 us`, with
`CH8..CH15 FULL_OFF`. The trial completed despite three recoverable NACKs; physical direction,
step/lift amplitude, support-leg behavior, current/voltage, temperature and noise still require
the Operator's observation. No keys 2..4 were sent in this trial.

After that trial, the rest-gated baseline was implemented, rebuilt, and deployed to COM4. The
three images from `flasher_args.json` all reported `Hash of data verified`. A read-only boot probe
then confirmed the isolated project, PCA9685 `0x40 ACK`, `50 Hz / 400 kHz`, `CH0-CH7 READY`,
`CH8-CH15 FULL_OFF`, and `/OE HIGH`. The later soft-limit revision was rebuilt and passed a
32-step host sequence, but a subsequent flash attempt returned COM4 busy/`PermissionError(13)`
because a host Python console still held the port; it has not yet been deployed. No `rest`,
`demo`, or other movement command was sent to the soft-limit revision, so the earlier physical
demo result does not validate its new rest preamble or workspace limiting.

After that trial, the Operator reported that CH0/CH1 exceeded the intended hardware boundary.
The incident occurred on the then-current quad image, which only used the generic `500..2500 us`
candidate pulse window and did not contain the independent directional hip-stop table. The board
was stopped with `UART stop -> /OE HIGH`. The current worktree now contains the per-channel table,
preflight gate and runtime gate described above; the earlier revision had not been flashed or
physically tested.

Deployment of the independent-limit revision on 2026-09-13: the host verified `COM4` was present
and available, then used the generated `flasher_args.json` offsets to write bootloader/app/partition
table. A subsequent read-only `verify_flash` reported `digest matched` for all three images. After
reset, `stop`/`list` confirmed the safe `/OE HIGH` boot state. The host then sent `rest` and received
`rest pose reached; holding target; /OE LOW`; no digit/demo command has been sent to this revision.
The board is currently holding the rest pose for Operator trial. This confirms deployment and the
firmware rest gate, not physical motion direction, amplitude, limit clearance, current, or temperature.

## Latest soft-saturation revision and loaded trial (2026-09-13)

The default active step is a `40 mm` request, clamped by the `+35 mm` foot workspace boundary;
the lift phase first moves the active foot back `6 mm` and up `15 mm`, and each stance leg retreats
`8 mm`. If a leg would cross its own independent hip-pulse soft limit,
the evaluator clamps that leg to the closest safe foot-x at the current height. It does not call
`stop` for that condition; the completion log reports a `soft_limit_legs` bit mask.

- Focused C test plus 100 consecutive executions passed after this change.
- ESP-IDF build passed; `niulai_servo_zero_test.bin` is `0xd5930` bytes with `0x2a6d0` bytes free.
- The three generated images were flashed to COM4 and all three passed read-back `digest matched`.
- One unloaded `1→2→3→4` cycle completed `4/4`; a further three-cycle unloaded run completed
  `12/12`, with no demo rejection, runtime stop, NACK, timeout, or terminal failure. RF reported
  `soft_limit_legs=0x01`, demonstrating active-step saturation while the demo remained complete.
- One authorized loaded cycle: key 1/RF and key 2/LF completed; key 3/RR encountered repeated
  PCA9685 NACK/software-timeout failures at about `1548 ms`, exhausted bounded retries, and
  correctly raised `/OE HIGH`. This was an I²C/power-under-load failure, not a soft-limit stop;
  key 4 was not sent after the safe abort. A subsequent boot probe confirmed PCA9685 `0x40 ACK`
  and `/OE HIGH`.

## Latest high-clearance lift trial (2026-09-13)

The high-clearance image uses `lift_back=6 mm` and `lift=15 mm`; the UART start log confirmed
these values for all four keys. In one unloaded `1→2→3→4` cycle, all four keys were accepted and
completed. RF reported `soft_limit_legs=0x01`; the remaining legs completed with no demo rejection
or runtime stop. The run had two recoverable I²C errors during the rest preamble and ended with
`logical=607`, `attempts=609`, `success=607`, `nack=2`, `terminal_failures=0`; the runner then
sent `stop` and confirmed `/OE HIGH`. This validates the new trajectory and software path only;
actual clearance and leg direction still require the Operator's observation.

## Known limitations and next step

### Load-boundary sweep (2026-09-13)

Using the UART runtime profile without reflashing, the following physical trials were
performed with the same four-leg sequence:

- `15,250,600,250`: one cycle and then three consecutive cycles completed (`12/12`);
  the aggregate reported `logical=962`, `success=962`, `nack=2`, and
  `terminal_failures=0`.
- `15,200,500,200`: two cycles completed (`8/8`); `logical=595`, `success=595`,
  `nack=2`, and `terminal_failures=0`.
- `15,160,400,160`: failed on LR/key 4 at about `857 ms` after repeated hardware
  NACKs; bounded retries were exhausted and `/OE HIGH` was asserted. The first three
  legs completed, but this faster profile is not accepted under the present hardware
  power/bus boundary.

The current load-safe candidate is therefore `15,250,600,250`; this is an empirical
test result, not a guarantee of power integrity or actuator safety. The raw UART
output is appended to `experiments/logs/EXP-028-2026-09-13-soft-limit-trial.txt`.

## Rest-task stack hardening and 20 mm no-load verification (2026-09-13)

一次受力运行在静息前置阶段遇到瞬时 I²C 错误后触发 `rest_pose` 任务栈溢出；固件随后将该任务栈从
`3072` 增至 `4096` bytes，并重新通过 focused C test、控制台测试、全量 offline checks 和 ESP-IDF
构建。镜像 `0xd59a0` 已在 COM4 烧录并完成三段 `digest matched` 回读验证。

修复后的 `15 mm` 版本曾完成空载和一轮受力 `1→2→3→4`；当前 `20 mm` 版本的两轮失败均为受力测试，
都在 key 3/RR 前摆阶段出现连续 PCA9685 I²C 失败并安全 `/OE HIGH`，没有重启或再次栈溢出。该结果
不能归因于软件限位，也不能证明 20 mm 受力安全。

本轮真正不受力的 `20 mm` 测试于 2026-09-13 完成 `1→2→3→4`，四键均 accepted/completed，
`soft_limit_legs` 依次为 `0x01、0x00、0x00、0x00`，最终统计为
`logical=615 attempts=615 success=615 nack=0 timeout=0 other=0 retries=0 recovered_after_retry=0 terminal_failures=0`；
随后 runner 发送 `stop`，板卡确认 `/OE HIGH`。这证明本轮无负载下的软件路径和串口执行，不证明机械净空、方向或承重能力。

当前 `20 mm` 是既有足端工作空间 `z=-105..-80 mm` 内的上限；未在此 demo 中宣称实现小腿关节精确抬高 `60°`。

## Fast direct-target profile (2026-09-13, software experiment)

为验证“尽快重新建立支撑，减少倒地瞬间的总负载变化”这一待验证假设，当前候选 profile 将
三条支撑腿的后撤增至 `20 mm`，主动腿 lift、forward swing、search 使用 `120/300/120 ms`。
这是短时序控制，不是单周期跳转；阶段内部仍按 20 ms 采样，舵机自身的响应速度也不等于瞬时机械到位。

软件跃度门已恢复为 `250 us`，仍保留每通道独立髋限位、足端工作空间、IK、通用传输范围和整帧预检。
`100/240/100 ms` 候选曾完成无负载 4/4，但有 7 次可恢复 NACK；`80/180/80 ms` 在 key 1 阶段失败。
通过 UART 参数 `20,120,300,120` 的无负载循环完成 4/4 且统计无 NACK，当前将其作为默认 sweet-spot 候选。
该参数变化属于 HIGH RISK，不应据此推断 NACK 已由倒地导致。

当前 SEARCH 只是定时下放，不读取 KW10，完成不等于 contact；无编码器时足端状态是命令
估计，不是实测；IMU 也未接入。受力循环中的 I²C 失败表明供电/电流/公共地/线束和负载边界
仍未通过，不能继续在同一受力条件下盲目重复；下一步应先做这些单变量排查，再由 Operator
记录实际方向、步长、抬高、支撑腿行为、限位、电流/压降、温升、噪声和急停结果。

## Host-only 1.5x amplitude overlay (2026-09-13)

本轮按 Operator 要求只修改宿主机 [`tools/quad_ascii_walk.py`](../tools/quad_ascii_walk.py)，没有修改或
重烧 `servo-zero-test` 固件。该轮脚本默认发送 `30,9,13,120,360,120`；由于实时 `press` 的前两个 profile
字段会被固件按住时间映射覆盖，脚本在收到 key-up 后加入等效保持时间，使常规保持时长对应的主动步幅和
支撑后撤约为原来的 `1.5x`。例如原始 `500 ms`（固件步幅 `16 mm`）会转换为等效 `800 ms`（`24 mm`）。

现有固件的保持计时上限 `1200 ms` 和步幅上限约 `34 mm` 未改变；长按因此会饱和，不能声称所有时长都能
达到精确 `1.5x`。按键事件仍独立处理，组合腿和可恢复 NACK 队列逻辑不变。`python3 -m unittest
tools.tests.test_quad_ascii_walk -v`（8 tests）、`python3 -m py_compile tools/quad_ascii_walk.py
tools/tests/test_quad_ascii_walk.py`、`git diff --check` 和 `sh tools/run_offline_checks.sh` 均通过；本轮没有
烧录、串口运动或新的摔倒/NACK 实体数据。

## 30 mm high-clearance envelope (2026-09-13)

Operator 要求将小腿抬高目标从 `20 mm` 提高到 `30 mm`，以给大腿前后摆动留出更大的地面净空。
本轮同步修改四足 demo 的足端上限：`MAX_FOOT_Z=-70 mm`、`MAX_LIFT=30 mm`；起始足端仍为
`(0,-100) mm`，因此 profile `30,9,30,120,360,120` 的抬腿段目标为 `z=-70 mm`。通用固件默认
仍保持 `12 mm`，只是允许显式 profile 使用新的 30 mm envelope。

验证结果：带当前四个独立髋限位的纯 C quad demo test 通过；30 mm 前摆中点 IK 有效、足端达到
`z=-70 mm`，SEARCH 完成后回到 `z=-100 mm`；ESP-IDF v5.5.5 Ninja build 通过，镜像大小
`0xd7270`，应用分区剩余 `0x28d90`（16%）。主机默认 profile 已同步为 `30,9,30,120,360,120`。
`python3 -m unittest tools.tests.test_quad_ascii_walk -v` — 10 tests pass；`python3 -m py_compile
tools/quad_ascii_walk.py tools/tests/test_quad_ascii_walk.py`、`git diff --check` 和
`sh tools/run_offline_checks.sh` 均通过（全量离线门：33 tool tests、纯 C host tests、文档 123 文件/353
本地链接、31 实验索引和 13 Decision 索引）。

这是 HIGH RISK 运动范围变更。上述结果只证明 mock/构建/软件路径，不证明 30 mm 在实体机构上的净空、
堵转、电流、NACK、脚滑或摔倒表现；本轮未烧录、未发送实体运动命令。

## 50 mm envelope and 40 mm joint-test candidate (2026-09-13)

按 Operator 要求，先完成更高净空代码，再进行联合测试。四足 demo 足端工作区上限从
`z=-70 mm` 扩展到 `z=-50 mm`，`MAX_LIFT` 从 `30 mm` 扩展到 `50 mm`。起始足端为
`z=-100 mm` 时，后续联合测试使用：

- 30 mm：`30,9,30,120,360,120`（主机默认）；
- 40 mm：`30,9,40,120,360,120`（新增明确候选）；
- 50 mm：仅作为固件 envelope 边界，不作为本轮实体测试目标。

代码已补充 30/40 mm 足端路径和 50 mm envelope 边界的纯 C 测试覆盖。新镜像
`niulai_servo_zero_test.bin` 大小 `0xd7270`，SHA-256 为
`61ff21d09563cab8d4d4157e9350070bf94a4d57d0d13044e02d3c435c209ccb`；构建、C quad test、11 项
主机 quad 测试、`py_compile` 和全量 `sh tools/run_offline_checks.sh` 均通过。随后按 40 mm profile
完成一次 RF 单步实体测试，又完成一次 `RF → LF → RR → LR` 四步实体测试；四步均
`accepted/completed`、`lift=40.0 mm`、`soft_limit_legs=0x00`，最终 `stop → /OE HIGH`；四步后的
无复位 stats 为 `logical=195 attempts=195 success=195 nack=0 timeout=0 other=0 retries=0
recovered_after_retry=0 terminal_failures=0`，UART 流未出现 NACK、timeout 或终止性 rejected。
这证明一次 40 mm 四步 UART/固件路径完成，不等于机身前移、脚滑、堵转电流、净空或摔倒已经验收；50 mm 仍只是
envelope 预留，不宣称当前候选 q→us 和机械限位可运行到 50 mm。

## Knee-dominant half-hip diagnostic (2026-09-13)

为区分“足端路径减小”和“髋关节角度真正减小”，本轮没有修改或重烧固件，改用现有的
`demo rf-lift-cosine-drop` 关节顺序诊断：CH1 先抬腿约 `45°`，CH0 只摆动名义 `22.5°`，即
原 `45°` 髋摆幅的一半。以 `45/70 mm` 模型和 `(0,-100) mm` 静息足端计算，固定髋角抬膝后
足端为约 `(37.468,-61.705) mm`，相对 `z=-100 mm` 的理论净空约 `38.295 mm`。

宿主机原生 PowerShell 在 COM4 完成 `stop → rest direct → stop → stats reset →
demo rf-lift-cosine-drop 22.5 → release → stop → stats`；固件报告 `CH1 fast lift=45.0°`、
`CH0 cosine swing=22.5°`、`motionleg touchdown complete`，统计
`logical=241 attempts=241 success=241 nack=0 timeout=0 other=0 retries=0
recovered_after_retry=0 terminal_failures=0`，最终 `/OE HIGH`。这证明小腿主导的约 38 mm 抬高
在软件/串口路径上可完成；模型 x 点约 `37.5 mm` 超出 quad demo 当前 `+35 mm` 足端软限位约
`2.5 mm`，因此不能直接把该单腿诊断当作四腿工作空间或实际离地高度验收。完整记录见
[`EXP-028-2026-09-13-half-hip-knee-lift.txt`](logs/EXP-028-2026-09-13-half-hip-knee-lift.txt)。
同一命令随后重复实测一次：动作仍完成并最终 `/OE HIGH`；本次首尝试出现 1 次 PCA9685 NACK，
由固件重试恢复，统计为 `logical=214 attempts=215 success=214 nack=1 timeout=0 other=0
retries=1 recovered_after_retry=1 terminal_failures=0`。这说明通信保护路径允许可恢复 NACK 继续动作，
但也说明当前实体总线仍有偶发干扰，不能将单次成功等同于长期供电/机械稳定性。

## Knee-dominant live-hold audit (2026-09-13, software only)

本轮按 Operator 要求只做代码审计、修正和软件验证，明确未烧录、未发送 UART 运动命令、未驱动实体。
审计发现并修正了三个会偏离玩法的问题：

1. 键盘 `press` 原先的默认回退 profile 仍是 `lift_ms=120`、髋比例 `50%`，与“小腿先抬高、
   大腿只小幅摆动”不一致；现在固件和 `tools/quad_ascii_walk.py` 统一默认
   `30,9,40,360,360,120,25`。
2. 实时路径原本以固定 `swing_ms`/保持时长结束，长按会停止；现在 `swing_ms` 只定义名义速度，
   髋摆动从膝抬升完成后持续增加，直到 key-up 或该腿自己的软件脉宽限位。释放会保存实际接受的
   摆幅，固定髋角并只执行 `SEARCH` 膝下放。
3. 长按限位搜索原有一个人为的 `200 mm` 扫描上限；现在改为按第一个安全脉宽边界做指数 bracket +
   二分，不再引入独立的游戏距离上限。启动时还会预检膝抬升端点，并把 live 初始化和输出使能放入同一
   临界区，避免先跑一帧旧的定时轨迹。

当前仍明确保留的边界：`demo quad N/mask` 是固定一次性诊断路径，不是键盘玩法；键盘组合最多两腿，
第二腿只能在第一腿的膝抬升窗口加入；`300..3000 us` 仍是候选传输范围，CH1/3/5/7 尚无已批准的
机械止挡；无编码器、KW10/IMU 不进入落脚决策；持续 I²C 重试耗尽仍会安全拉高 `/OE`，这是终止性故障
保护而非把后续命令静默吞掉。

软件验证（工作区未提交）如下：

- `cc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware-esp32/tools/servo-zero-test/main firmware-esp32/tools/servo-zero-test/main/servo_zero_leg_math.c firmware-esp32/tools/servo-zero-test/main/servo_zero_quad_demo.c firmware-esp32/tools/servo-zero-test/tests/test_servo_zero_quad_demo.c -lm -o /tmp/niulai-servo-zero-quad-demo-test && /tmp/niulai-servo-zero-quad-demo-test` — pass；覆盖膝先抬、40 mm、默认 25% 髋、长按无时限、201 mm 无人为距离截断、释放冻结髋、LF+RR 支撑腿均分后撤、独立限位和连续 32 步模拟。
- 同一 C test 使用 `-fsanitize=address,undefined -fno-omit-frame-pointer` 构建并运行 — pass；无 ASan/UBSan 报告。
- `python3 -m unittest tools.tests.test_quad_ascii_walk -v` — pass，12 tests；覆盖默认 profile、可恢复 NACK 不误判、终止性故障不伪装成 busy、release 完成竞态。
- `python3 -m py_compile tools/quad_ascii_walk.py tools/tests/test_quad_ascii_walk.py` — pass。
- `ninja -C firmware-esp32/tools/servo-zero-test/build-servo-zero` — pass，ESP-IDF v5.5.5；镜像 `0xd8a30`，应用分区余量 `0x275d0`（15%）。构建工具仍报告既有 `ninja: corrupt build log: missing field` 警告，但编译、链接、镜像和分区检查均完成。
- `sh tools/run_offline_checks.sh` — pass，35 tool tests、纯 C host tests、文档链接/索引和 whitespace 检查均通过。

本轮完整复核命令、参数、结果和未验证风险见
[`EXP-028-2026-09-13-live-hold-audit.txt`](logs/EXP-028-2026-09-13-live-hold-audit.txt)。

## Knee-dominant live-hold flash and PowerShell smoke test (2026-09-13)

Operator 要求烧录并确认宿主机 PowerShell 可直接测试。使用分支工作区构建的 ESP32-S3 镜像通过 Windows
原生 `COM4` 烧录；bootloader、application、partition table 均返回 `Hash of data verified`，随后硬复位。
镜像 SHA-256 为
`a0f219c4250e513f65faaceb7d8811d6a7a45ac91b99d0f0f648c0dbcc432abd`。

随后使用 `tools/quad_ascii_walk.py --sequence 1` 完成 RF 单腿冒烟测试，再使用
`--sequence 1234` 完成 RF/LF/RR/LR 四步实时路径冒烟测试。两次均完成 `rest direct → press → release →
complete → stop`，UART 未出现 NACK、timeout 或终止性 rejected；四步最后一次报告
`soft_limit_legs=0x03`，表示累计支撑后撤触到 RF/LF 软件工作边界后被夹持继续执行，并非动作失败。
板卡最终为 `/OE HIGH`。这证明了 PowerShell→COM4→固件的通信和状态流程，不证明实体机身前移、脚滑、堵转或
摔倒已经验收。完整命令和回显摘要见
[`EXP-028-2026-09-13-knee-dominant-live-hold-flash.txt`](logs/EXP-028-2026-09-13-knee-dominant-live-hold-flash.txt)。

下一步必须先由 Operator 审阅差异，再按 HIGH RISK 规则完成机械固定、八路限位/方向、6 V 限流、人员
净空和急停确认，最后另行授权烧录与低幅单腿台架测试；本轮没有把软件通过写成实体走行、NACK、脚滑或摔倒通过。

## Half-hip integration candidate (2026-09-13, software validation)

为把单腿“膝主导抬高、髋减半”方案纳入四足步态，隔离 demo 的 profile 新增第 7 项
`hip_scale_percent`。`50` 表示只将主动摆动腿相对静息足端的髋角度偏移缩小一半；膝在原目标
`z` 上重新求解，支撑腿改为按主动足端实际 FK 前移总和及明确支撑掩码计算同一个名义后撤请求，再由每条支撑腿按自身容量独立裁剪。若关节重分配使实际足端越过
`x=-35..+35 mm`，实现会在同一高度搜索最近安全目标点，并继续执行，不会把整条四足帧判为
`rejected`。

纯 C 预检覆盖 `30,9,40,120,360,120,50` 的 RF、LF、RR、LR 单腿以及 LF+RR 对角组合；
四腿均能生成完整有效路径，且受当前独立髋限位表约束。该项目前只记录软件/候选 q→us
验证。重建烧录后，从 `rest direct` 开始的 `RF→LF→RR→LR` 总装实体序列四步均完成；
同 profile 的 LF+RR 对角实体试验在约 270 ms 因 PCA9685 连续 NACK 终止，未继续执行
RF+LR。实体净空、前移、脚滑、堵转电流和摔倒仍不是 UART 或软件预检可证明的结论；
完整结果见 [`EXP-028-2026-09-13-half-hip-four-leg-integration.txt`](logs/EXP-028-2026-09-13-half-hip-four-leg-integration.txt)。

## Deep live-hold review and correction (2026-09-13, software only)

Operator 提供的故障现象是：单腿持续前摆时小腿偶尔向后翻并报告“参数或安全检查未通过”；主动腿前移速度明显小于三条支撑腿后撤；有时单腿动作只有一或两条支撑腿后撤。以下是代码层复盘结论，均不等同于本轮实体观察。

### 深层原因

1. **把请求量当成实际运动量。** 实时状态保存的是键盘按住时间换算出的 `swing_offset_mm`，旧分配器直接使用它计算支撑后撤。可是主动腿髋只执行 `hip_scale_percent=25` 的髋角偏移，而且还可能被该腿单向脉宽限位夹住；因此主动足端实际只走了请求距离的一部分，支撑腿却仍按完整名义距离后撤，表现为主动腿“跟不上”。
2. **支撑腿逐条裁剪，破坏了全局同步。** 旧逻辑对每条支撑腿分别把 `base_x - retreat` 夹到自己的足端/髋脉宽安全边界。四条腿的静息脉宽和方向限位不对称，某一条先到边界后，它的目标不再继续变化，而其它腿仍继续后撤，于是出现一条、两条或三条腿不一致。这个行为并非玩家输入随机，而是 per-leg clamp 与“共同支撑动作”语义冲突。
3. **实时路径复用了固定诊断路径的预检。** `press` 先构造固定时长 `SETTLE → LIFT → SWING → SEARCH` 路径；实时玩法实际是 `LIFT → 按住摆动 → key-up → SEARCH`。两者的目标、时序和支撑累计不同，固定路径的不可达/限位结果可能错误地否决一个实时上仍可达的按键。
4. **IK 只看端点，不看连续分支。** 固定髋抬膝和摆动期间，`servo_zero_leg_knee_at_height()` 会在两个候选解中按参考角选择最近者；在角度跨越、数值包络或镜像映射不一致时，单个端点可能仍通过 IK，但中间的膝分支/小腿方向可能发生跳变。这正是“向后翻”必须优先作为软件连续性故障处理的原因，而不是仅看最终 `valid`。
5. **共享状态的判定与更新不在同一锁内。** 旧 `press/release` 入口先读全局动作/掩码，再由下层函数重新获取 I²C mutex；键盘快速连发时，UART parser、20 ms 动作任务和入口可能看到不同代的状态，导致误报 busy/rejected 或重复处理 key-up。这个竞态不会制造一个合法的机械分支，但会放大前述错误的可见频率。

### 当前修正

- 每个 20 ms tick 先求主动腿的实际 FK 足端位置，再以“实际主动 `+x` 位移总和 ÷ 明确支撑掩码数量”计算共同支撑后撤；不再用名义 `swing_offset_mm` 直接驱动支撑。
- 支撑掩码固定为 `(~moving_leg_mask) & 0x0f`。每个 tick 先按实际 FK 总驱动量得到同一个名义 body-frame 后撤请求，再由每条支撑腿独立按自身安全容量裁剪；若某条先到边界，只标记并冻结该支撑腿，其他支撑腿继续执行各自可用的后撤距离。
- 实时 `press` 使用专用 live plan builder，只检查共享几何/配置，启动时另行检查当前快照和膝抬升端点；不会被固定诊断路径的终点预检误拒绝。固定 `demo quad N/mask` 仍保留完整路径预检。
- 实时主动摆动沿单调前向路径扫描 32 个样本，要求膝分支不变、相邻膝角不跳变、足端 `x` 不回退；遇到第一处不安全边界时指数 bracket + 二分锁存在最后安全偏移，不扫描后续安全岛，避免长按突然跳跃。
- `press/release` 的状态读取、掩码更新与必要的 rebase 放入同一 mutex 保护；可恢复的单次 NACK 仍由底层帧重试处理，只有重试耗尽才终止 quad 动作。

### 专门回归测试集

本轮离线测试明确覆盖 Operator 提示的三种压力场景：

- 单腿一直前摆：RF/LF/RR/LR 分别从 `offset=0..160 mm` 连续采样，检查实际足端 `x` 单调不回退且膝分支保持；另检查长按直到第一安全边界不因固定 `swing_ms` 自动停止。
- 双腿一直前摆：LF+RR 组合同时保持，检查两条主动腿均有效、两条支撑腿使用同一实际总驱动的均分距离。
- 快速连续四腿前摆：16 次轮换 RF/LF/RR/LR 的 lift→swing→search，使用上一帧成功足端/脉宽作为下一次起点，检查每次动作仍可达且支撑掩码始终为 moving mask 的补集。
- 额外检查单个非对称支撑限位：人为让某一支撑容量更小，确认受限支撑腿的实际后撤量更小并标记其 soft-limit bit，其他支撑腿继续执行同一个名义请求。

本轮不烧录、不打开串口、不驱动实体；因此“小腿实际是否向前/向后翻、是否摔倒、前移速度、NACK 频率”仍必须在完成机械固定、限位/方向核对、限流和急停确认后，由 Operator 做单腿→双腿→快速四腿的实体分级验证。测试命令和结果记录在 [`EXP-028-2026-09-13-live-hold-audit.txt`](logs/EXP-028-2026-09-13-live-hold-audit.txt)。

## Foot-space conservation repair (2026-09-13 night, superseding the live semantics above)

Operator 明确否决“某条支撑腿先触限则所有支撑腿一起停止”；本轮继续保留每腿独立容量裁剪。修复后的无裁剪数学不变量是
`Σ active net Δx + Σ support Δx = 0`：一条移动腿的完整净 `+x`（含 LIFT/SWING/SEARCH）由三条支撑腿均分，两条移动腿的净位移和由两条支撑腿均分。发生单腿裁剪时只冻结该腿，不伪称仍满足全局守恒。

实时曲线现为：膝关节沿指定方向先抬（固定髋 FK 弧）→ 足端在固定 Z 水平前摆（完整 IK）→ 松键后保持 X 下放（完整 IK）。若在水平前摆前松键，则反向重走抬腿弧。审计发现旧 nearest-root 在小腿近似竖直、同一 Z 存在两个同号膝解时会选反向根，随后跳支，现已由显式有方向的 knee-lift solver 修复。支撑容量也改为从当前位置向后逐步寻找第一个不安全点，不能再由远端“重新安全”的点掩盖中间脉宽违规。

为容纳从 `(0,-100)` 抬高 40 mm 后约 `(38.13,-60)` 的膝主导弧线，实时 X 候选包络改为 `-50..+50 mm`。UART 字段 `hip_scale_percent` 为保持兼容仍保留名称，但实时路径中改为缩放水平足端速度；固定诊断路径仍保留原髋角缩放语义。该差异已写入 README/固件 help。

离线证据：严格 leg-math 与 quad C 测试、ASan/UBSan、64 步快速轮换、focused 20 轮重复、全仓 `tools/run_offline_checks.sh`、ESP-IDF v5.5.5 build 和 `git diff --check` 均通过。支撑安全扫描只运行到本帧请求距离，不再每 20 ms 扫完整工作区。镜像仅构建为 `0xd93d0`，没有烧录、串口探测或运动。四腿物理零位、q→us、方向和机械限位仍未由编码器/量具标定，故本结果只证明模型内部连续性与守恒，不证明实体必然前移。完整数值和命令见同一 [`live-hold audit log`](logs/EXP-028-2026-09-13-live-hold-audit.txt)。

## Recoverable-NACK SEARCH timing and bent-knee touchdown repair (2026-09-14, software only)

Operator 日志中的 NACK 没有连续耗尽四次：底层重试已经恢复，真正终止来自随后独立的相邻帧保护 `CH1 769->1165 us`。根因是 SEARCH 以墙钟 `quad_elapsed_ms` 计算进度；I²C 重试和任务延迟消耗了真实时间，下一次有效迭代会直接追赶 120 ms 终点，从而跳过中间脉宽帧。SEARCH 现仅在动作任务成功完成一次控制迭代时推进一个 20 ms 逻辑帧，默认仍为 120 ms；可恢复总线延迟不会再压缩轨迹。

落脚几何也按 Operator 澄清修改。水平摆动后的终点不再保留包含膝抬升弧线在内的 `release X`，而是
`takeoff X + accepted horizontal swing offset`：120 ms 内同时收回抬膝造成的临时前向 X 并下降到起脚 Z，保留玩家按住期间真正的水平步幅。纯 C 回归比较旧终点，确认新终点膝角至少多弯 `0.1 rad`，且 20 ms 相邻脉宽变化不超过 `125 us`，低于 `250 us` 保护阈值。短按未进入水平 SWING 时仍原路反向重走抬膝弧。

宿主脚本在 press/release 前不再静默丢弃异步终止日志；若固件已停止，它会清除本地 ready 状态并明确提示重新按 `S`。严格 leg-math/quad C、Python 14 项、ASan/UBSan、`sh tools/run_offline_checks.sh`、ESP-IDF v5.5.5 build 和 `git diff --check` 通过；镜像 `0xd9490`，应用分区余量 `0x26b70`（15%）。本节未烧录、未访问串口、未驱动实体。

### Flash and blocked low-amplitude trial

经 Operator 授权，先终止占用 COM4 的旧 `quad_ascii_walk.py` PID 30540，再由宿主 PowerShell/native esptool 4.12.0 烧录镜像；bootloader、application、partition table 均 `Hash of data verified`，应用文件 SHA-256 为 `8f9bd6d9330da5d6ec2b4dc230dac9c3f89ea466c51549cd7a1dac56135c180b`。启动默认先回报 `/OE HIGH`，随后 I²C scan 无任何 ACK，PCA9685 `0x40` 的 MODE1 初始化写入连续 4 次返回 `ESP_ERR_INVALID_STATE`/NACK，固件进入 `initialization failed; /OE remains HIGH forever`。计划的 `8,4,30,360,360,120,25` RF 单腿动作没有进入静息位，也没有驱动舵机。该结果只证明镜像写入成功并由启动安全门正确阻止输出；必须先恢复 PCA9685 逻辑供电、公共 GND、SDA11/SCL12、上拉或接插件，再确认 `0x40 ACK` 后继续落脚验证。

接线恢复后，以同一镜像和 profile 重新通过宿主 PowerShell 运行 `tools/quad_ascii_walk.py --port COM4 --profile 8,4,30,360,360,120,25 --sequence 1`。启动确认 `0x40`、`0x68`、`0x70` 均 ACK，进入 direct rest 后 RF 回报 `press accepted`、`release accepted`、`quad demo complete`；未观察到 NACK、timeout、invalid pose 或终止性 rejected，最后脚本发送 `stop` 并关闭输出。该次只验收 RF 低幅 UART/状态闭环；弯膝落脚角度、净空、脚滑、机身前移、堵转电流、温升和摔倒仍需 Operator 现场观察。

## LF zero-swing repair and 20% speed increase (2026-09-14, software only)

Operator 观察按键 `2=LF` 单独操作时只有抬腿、几乎没有主动前摆，而 LF 作为其它腿动作的支撑腿时能够后撤。新增针对当前镜像 q→us 和 Operator 髋限位的回归后，`2 mm` 名义 LF 前摆请求只能接受约 `0.0001 mm`，复现了该现象。

根因是上一版 bent-knee touchdown 预检把抬高后的 release 点与触地点用足端直线相连。该直线中段先把 LF 髋角带向反方向；LF 的 CH2 映射为镜像斜率，静息 `1672 us` 到已知反向上限 `1750 us` 只有 `78 us`，所以任何非零水平摆幅都被预检压到近零。LF 作为支撑腿走的是同高度水平后撤路径，不经过这条直线，因而仍会随其它腿运动。其它腿的方向和限位裕量不同，所以没有在同样小的请求上首先表现为零；40 mm 抬高下仍可能存在同类潜在风险。

SEARCH 现改为沿原 knee-first 抬腿弧反向下降，并在整条曲线上保持已获得的水平摆动偏移：起点精确等于 release pose，终点仍为 `takeoff_x + deliberate_swing`。这同时满足 120 ms 固定落脚、撤销抬膝临时 X、触地膝保持弯曲和不穿越 LF 反向髋限位。新增测试要求 LF 单独长按在 `2..40 mm` 请求范围内足端 X 严格增加、CH2 脉宽严格下降、支撑掩码固定为 `0x0d`；旧测试只检查 `pose.valid`，无法捕获“有效但零位移”的玩法故障。

默认 profile 的 `swing_ms` 从 `360` 调整为 `300 ms`，使水平摆腿速度提高 20%；`lift_ms=360` 和 `search_ms=120` 不变。严格 quad C、ASan/UBSan、Python 14 项、全仓 `tools/run_offline_checks.sh`、ESP-IDF v5.5.5 build 和 `git diff --check` 全部通过；新镜像 `0xd9530`，应用分区余量 `0x26ad0`（15%），SHA-256 `8a07c132c9aa740c3991f50194b26b00cd02bc574ff0eb2c69c6fee3cbba7c5b`。构建仍显示既有 `ninja: corrupt build log: missing field` 警告，但编译、链接、镜像和分区检查完成。本节只进行离线代码、测试和构建，不访问 COM4、不烧录、不复位、不发送运动命令。
