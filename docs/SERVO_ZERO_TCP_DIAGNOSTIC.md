# Servo-zero TCP 诊断通道

Status: **Proposed bench diagnostic interface**
Last reviewed: 2026-09-13

## 目的

`firmware-esp32/tools/servo-zero-test/` 可在不连接 USB 的情况下，把 ESP32
的 `ESP_LOG` 输出发送到一台连接到 ESP32 临时 Wi‑Fi 热点的电脑。电脑也可以
通过同一条 TCP 连接发送原有 ASCII CLI 命令并接收执行日志，因此这条链路
主要用于远程获取调试信息，不是正式比赛控制接口。

这是隔离台架固件的诊断能力；它不接入正式 BLE、FSM 或执行器 adapter。

## 默认参数

| 项目 | 默认值 |
| --- | --- |
| SoftAP SSID | `牛来快跑-DIAG` |
| SoftAP password | `牛来快跑-Diag` |
| ESP32 AP address | `192.168.4.1` |
| TCP port | `3333` |
| Command framing | UTF-8/ASCII text, one command per LF |

SSID、密码和端口可以在 `menuconfig` 的 isolated CH0 servo-zero test 菜单中
修改。密码仅用于本地台架隔离，不提供身份认证或加密安全边界；不要把此接口
暴露到办公/互联网网络。

## 主机使用

1. 保持舵机动作的固定、限位、供电/限流、人员净空和急停条件满足；初次连接
   时固件仍保持 `/OE HIGH`，不会自动启动动作。
2. 烧录启用 TCP 诊断的 `servo-zero-test` 固件，等待串口启动日志中的
   `TCP_DIAG_AP`。
3. 在 Windows、Linux 或 macOS 上连接 `牛来快跑-DIAG` 热点。
4. 在仓库根目录运行：

   ```bash
   python tools/servo_zero_tcp_console.py --host 192.168.4.1
   ```

   输出窗口会持续显示 ESP_LOG；输入 `stats`、`stats reset`、`list`、`help`
   或已有的台架命令后回车即可发送。退出时工具默认尽力发送 `stop`；仅查看
   日志且不希望这一行为时使用 `--no-stop-on-exit`。

   也可以让工具发送命令后持续记录日志：

   ```bash
   python tools/servo_zero_tcp_console.py \
     --command "stats reset" \
     --command "pose pseudo 35" \
     --duration 8 \
     --output experiments/logs/servo-zero-tcp.log
   ```

   要执行当前的 50 轮诊断基准（USB 拔除、外部 3.3 V、400 kHz、8 个舵机、
   MPU6050 断开），在确认机械固定、供电和急停后运行：

   ```bash
   python tools/servo_zero_tcp_console.py \
     --benchmark 50 \
     --benchmark-amplitude 35 \
     --benchmark-timeout 8 \
     --output experiments/logs/EXP-019-wifi-400khz-50-rounds.log
   ```

   工具会先发送 `stop`、`list`、`stats reset`，然后逐轮发送
   `pose pseudo 35`，等待 `rest pose reached`，发送 `stop` 并等待
   `/OE HIGH`；最后发送 `stats`。每一轮以及连接后收到的所有原始 ESP_LOG
   都会写入 `--output` 文件。重点查看最终的
   `I2C stats: logical=... attempts=... nack=... timeout=... other=...`
   以及对应的 `retries`、`recovered_after_retry`、`terminal_failures`。
   如果出现 timeout，原始日志应包含该次的 `SDA=... SCL=...` 快照；若某轮
   未收到完成标记，工具会立即发送 `stop` 并以失败退出。

TCP 诊断命令和 UART ASCII CLI 共用同一个 ESP32 主任务解析器；每条远程命令
会收到 `TCP_DIAG_COMMAND_BEGIN/END` 标记，命令产生的 `ESP_LOG` 会按产生顺序
流回连接的电脑。启动前已经产生的历史日志不会补发；应在连接后重新执行
`stats` 或重新开始一轮测试。

## 限制与验证状态

- 当前只有一个 TCP 客户端；新客户端不会抢占已有连接。
- 日志和命令队列是有界、非阻塞、尽力而为的。日志过多时可能丢失，固件仍
  继续运行；这不是实时控制或可靠日志存储。
- Wi‑Fi 会增加 ESP32 的射频活动和电流，可能改变 I²C/舵机实验条件。因此
  用 TCP 获取日志时必须在实验记录中标注“Wi‑Fi TCP 诊断开启”，不能把它的
  统计结果直接当成 USB 条件下的同一组样本。
- 当前固件已完成 ESP-IDF build、COM4 烧录，并从启动日志确认 SoftAP/TCP
  初始化成功；电脑连接热点后的 TCP 收日志、命令回包和长时间日志丢包行为
  仍需要 Operator 做实体验证。
- 50 轮自动化客户端已加入，但上述 benchmark 结果尚未产生；实际运行时必须
  记录“USB 已拔除”和 Wi‑Fi TCP 诊断开启，因为 Wi‑Fi 本身会改变实验条件。
- 任何实体动作仍必须在安全台架上从低幅度开始，并随时可以发送 `stop` 和
  切断舵机 6 V。TCP 通道无认证，不能无人值守运行。
