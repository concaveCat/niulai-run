# EXP-003 KW10 GPIO39 一针电气检查

## Objective

验证制造板上 KW10 的 C-NC 接线与 ESP32-S3 GPIO39 的一针电平映射，为四路去抖台架测试提供基线。

## Hypothesis

使用 GPIO39 内部上拉时，C-NC 开路（按下）应读到高电平并输出 `1`；C-NC 短路（释放）应被拉到低电平并输出 `0`。

## Hardware version

- ESP32-S3 N16R8 / USB Serial-JTAG，测试端口 `/dev/ttyACM0`。
- 已生产 Carrier 板和 KW10 按钮；C 接 GND，NC 接 GPIO39。
- 测试期间仅连接 USB，舵机/6 V 电源保持断开。

## Software commit

验证时任务分支为 `fw/kw10-perfect-touchdown`，HEAD 为 `6a13310`，工作区含未提交修改，应用版本为 `6a13310-dirty`；对应固件实现现已记录在提交 `06b14e3`。测试镜像为 `firmware-esp32/build/niulai.bin`，SHA-256 为 `637f02f912f4542e32e76354a6327d06e7a3c559822e251a5ab5ac818a3ef1b6`。

## Parameters

- `CONFIG_NIULAI_GPIO39_CLI=y`；`CONFIG_NIULAI_AUDIO_TEST` 关闭。
- GPIO39 输入、内部上拉、10 ms 轮询；CLI 直接输出 GPIO 电平，不包含去抖。
- UART 115200 baud；烧录 bootloader、partition table 和 app 后硬复位。

## Test method

1. 构建并烧录隔离 GPIO39 CLI 镜像。
2. 连接串口监视 120 秒，按压/释放 KW10，观察输出边沿。
3. 完成本轮操作后拔掉 USB。

本次完成 1 轮串口监视；监视期间累计约 11,687 个采样，期间观察到多次 `1↔0` 转换，随后稳定为 `0`。

## Video / logs

无照片或视频。原始 UART 输出保留在本次协作会话的工具记录中，未另存独立日志文件；关键输出为 `GPIO39 state=1`（开路）和 `GPIO39 state=0`（短路）。

## Result

- 正确镜像实测通过：开路/按下输出 `1`，短路/释放输出 `0`。
- 串口监视最后因 Operator 拔线触发 `device disconnected or multiple access on port`；这是测试结束动作，不是 GPIO 或固件故障。
- 首次烧录镜像因本地 `sdkconfig` 残留 `CONFIG_NIULAI_AUDIO_TEST=y` 进入 `audio_test`；关闭该选项后重新构建、烧录并完成上述 GPIO39 验证。
- 本实验只确认一针原始电平极性，未测量机械抖动持续时间，也未确认四路安装后的触地方向。

## Next step

启用隔离 `CONFIG_NIULAI_FOOT_CONTACT_TEST`，按当前映射在 GPIO39=FR、GPIO40=FL、GPIO41=RR、GPIO42=RL 上记录 `gpio/raw_contact/debounced`，分别验证四路机械方向，并以实际边沿数据调整去抖候选值。
