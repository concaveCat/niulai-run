# Decision: ESP-IDF 项目基线（ESP32 固件端点）

Status:
- Testing

Problem:
本 Decision 提出时，仓库只有 Python 控制 mock（`firmware/`）和 Android BLE 控制器，尚无可版本化的 ESP32 固件构建系统。此前 README 和早期协作记录中的“尚无 ESP-IDF endpoint”属于当时状态；2026-09-05 团队曾在分支上验证过一版 ESP-IDF v5.5.5 + ESP-NimBLE 端点（GATT 注册、广播、UART 掩码接收），但源码未入仓、且 failsafe 任务栈曾溢出。当前 `firmware-esp32/` 已进入工作区，下面的 Result 记录其软件基线仍待硬件验收的状态。

Options:

1. **ESP-IDF v5.5.5 + ESP-NimBLE**：与已硬件验证的栈一致；`docs/BLE_PROTOCOL.md` 明确指定；组件化 + Kconfig 适合安全分级；本机可经 ghfast.top 镜像 + uv 无 root 安装。
2. **Arduino-esp32 core 3.3.10**：本机已有完整工具链、可立即编译；但与团队验证栈分叉，配置分级和组件边界弱，后续合流需返工。
3. **PlatformIO**：第三方包管理，对 GitHub 依赖重（本机网络受限）。

Current hypothesis:
采用 ESP-IDF v5.5.5 + ESP-NimBLE。理由：与 2026-09-05 已验证端点同栈、协议文档已按此指定、Kconfig 三级门控（BLE_ONLY / BLE_VERIFIED / ACTUATION_ENABLED）能强制"看门狗验收前 BLE 输入不接触舵机"；纯 C 组件（leg_fsm / ble_protocol / control_glue）保持硬件无关，可由主机 gcc 直接测试。

Experiment:
1. 本机：镜像克隆 + 工具链引导后 `idf.py build` 通过；host 测试（纯 gcc）镜像 Python 6 例并补齐 tick 顺序边界用例，全部绿。
2. 硬件现场：烧录后 UART 日志（`BLE_GATT_REGISTER`、广播名）与协议文档逐字对照；手机端 0x01/0x09/0x08/0x00 掩码验收；500 ms 看门狗复测（failsafe 任务 stack-depth 参数 4096 bytes）；BLE_VERIFIED → ACTUATION_ENABLED 分级验收（含验收窗口与保险丝/限位前提）。

Result:
- 本机（WSL，无 root）：ESP-IDF v5.5.5 经 ghfast.top 镜像克隆成功；CMock/openthread 两组嵌套子模块（仅单元测试框架/Thread 协议栈，构建不需要）因镜像 URL 改写缺陷被禁用；cmake 4.4.3/ninja 1.13.2 经 uv 安装；xtensa-esp-elf/xtensa-esp-elf-gdb/esp-rom-elfs 工具链经官方 github_assets 镜像就位（riscv32/ULP/openocd 未装，仅调试/ULP 需要，`export.sh` 因此暂不可用，构建用 `IDF_PYTHON_ENV_PATH=~/esp-idf-venv` + xtensa bin 手工环境配方）。
- `idf.py build`（clean）通过：`niulai.bin` 0x7fdc0 字节，app 分区余量 50%。
- 集成审阅后的 ESP-IDF v5.5.5 clean build 通过：`niulai.bin` 0x80d50 字节，app 分区余量 50%。
- host 测试（纯 gcc，`tests/host/run_host_tests.sh`）23 个用例、211 项检查全绿，含 Python 6 例镜像、tick 顺序边界、BLE 包校验矩阵、UUID 字节序及远端输入状态边界测试；四个纯 C 组件纯度检查零匹配。
- 硬件 bring-up 已部分完成：ESP32 endpoint 已烧录，UART 已对照 GATT 注册、广播服务/特征和 `0x01/0x09/0x08/0x00` 控制掩码；failsafe 任务栈修正后的固件也已重新构建/烧录。仍待一次干净的手机/ESP32 验收，覆盖 STATUS UI、断连恢复、500 ms 看门狗和持续连接；`BLE_VERIFIED → ACTUATION_ENABLED` 分级验收及 bonding/authentication 仍未完成。

Decision:
未冻结。软件侧基线已验证（Testing），硬件验收通过后转 Accepted。

Consequences:

- 新增 `firmware-esp32/` 子树，与 Python `firmware/` 并存；控制层保持硬件无关（纯 C 组件不含任何 ESP/FreeRTOS 头文件）。
- 活动腿（单腿 FSM 驱动哪条腿）是编译期 Kconfig 常数，属静态原型接线配置，不是自动选腿（自动选腿在 v0 被禁止）。
- Kconfig 的三级安全门控选择默认仍为 `BLE_ONLY`；公共 `sdkconfig.defaults`
  也保持 `BLE_ONLY`。已确认固定腿台架 profile 通过
  `sdkconfig.bench.defaults` 显式叠加为 `ACTUATION_ENABLED`，舵机输出仍
  必须通过运行时验收窗口。
- host 测试以纯 gcc 运行，可低成本进 CI；on-target unity 测试留待后续。
- BLE 的加密 bonding / no-PIN 台架策略由 [`Decision 017`](017-no-pin-actuation-bench-profile.md)
  记录；这不等于 MITM authentication，生产安全策略仍未冻结。
- `OPEN_DECISIONS.md` 中"eventual ESP-IDF project baseline"一项由本记录解决。
