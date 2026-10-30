# 音频听测 Runbook（MAX98357A + 扬声器）

状态：**Active / 软件与构建已验证，实体听感待台架确认**

目的：验证内嵌 `牛来叫妈妈.mp3` 经 Helix 解码、I²S 输出、MAX98357A 功放和扬声器的完整链路。

本 runbook 不修改 BLE / FSM / 舵机边界，不产生任何实体运动。

---

## 1. 前提

- ESP-IDF v5.5.x；
- ESP32-S3 开发板通过板载 USB 连接，确认本次枚举的 `COMx` / `/dev/ttyACMx`；
- 按 Gerber 网络接线：
  - `DIN=GPIO4`、`BCLK=GPIO9`、`LRC/WS=GPIO10`；
  - `SPK+` / `SPK-` 是 BTL 输出，**两线都不得接地**；
- MAX98357A 由载板 3.3 V 供电；主电限流接入；**舵机 6 V 一路断开**。

---

## 2. 上电前安全预检

1. 舵机电源断开；听测不需要任何执行器动作。
2. `/OE` 由固件启动第一动作拉高（GPIO1，外加上拉）。
3. 主电源限流；急停/断电方式就位。
4. 扬声器固定，线材不受力；确认标称阻抗/功率（设计假设 4 Ω / 2 W，不确定时只测低音量）。
5. 已知现象：模块 GAIN/SD 未引出时无法固件静音；I²S 启停可能有轻微爆音。

---

## 3. 构建（先 10% 音量）

```bash
cd firmware-esp32
idf.py -B build-audio10 \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.audio-listen.defaults" \
  set-target esp32s3 build
```

`sdkconfig.audio-listen.defaults` 只覆盖：

```text
CONFIG_NIULAI_AUDIO_VOLUME_PERCENT=10
```

---

## 4. 烧录与启动确认

```bash
idf.py -B build-audio10 -p /dev/ttyACM0 flash monitor
```

启动日志应出现类似：

```text
AUDIO_READY command=audio play alias=NIULAI asset=牛来叫妈妈.mp3 volume_percent=10
```

`volume_percent=10` 证明 overlay 生效；如果仍为 50，说明 defaults 叠加顺序有误，停止并检查构建命令。

---

## 5. 听测序列

在 monitor 串口依次输入：

| # | 命令 | 预期 | 记录 |
| --- | --- | --- | --- |
| 1 | `audio play` | 出声，约 4.4 s，尾音完整 | 是否出声？音量？失真/破音？ |
| 2 | 播放中 `audio stop` | 尽快停止（块边界） | 是否立即停？ |
| 3 | `audio play` 连发两次 | 第二次从头重播 | 是否重头？ |
| 4 | `NIULAI` | 与 `audio play` 等效 | 是否触发？ |
| 5 | 观察上电/启停 | — | 爆音？底噪？ |

---

## 6. 音量档

10% 干净后才升到 50%（默认）：

```bash
cd firmware-esp32
idf.py -B build-audio50 \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" \
  set-target esp32s3 build
```

重复第 5 节。50% 是当前默认值；更高音量需先核对 3.3 V 预算（功放+扬声器峰值约 0.7 A）和扬声器功率上限。

---

## 7. 结果记录

建议记录：

- 启动 `AUDIO_READY` 行；
- 每档音量的听感/失真/爆音；
- 扬声器标称阻抗/功率；
- MAX98357A 模块丝印（A/B 后缀）和供电实测电压；
- 任何异常、复位或 UART 报错；
- 对应 Experiment / 日期 / 操作人。

---

## 8. 边界

- 不修改 BLE/FSM/舵机代码或配置；隔离 profile 保持关闭。
- 不把语音 `VOICE_COMMAND` 接到播放器（需另立 Decision）。
- 烧录和上电是外部硬件动作；执行者必须先确认安全条件。
- 构建成功、解码成功、I²S 写出成功都不等于扬声器有声；以实际听测为准。

相关记录：[`EXP-030`](../experiments/EXP-030-embedded-mp3-command-playback.md)、[`BLE_PROTOCOL.md`](BLE_PROTOCOL.md)、[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)。
