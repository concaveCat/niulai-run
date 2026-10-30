# 纯 C host 测试

无需 ESP-IDF、开发板或舵机的回归入口。使用普通 `gcc`、C11 和仓库自带的极简 `CHECK` 宏。

```bash
sh firmware-esp32/tests/host/run_host_tests.sh
```

退出码非零即失败。脚本会：

1. 用 `-std=c11 -Wall -Wextra -Werror` 编译纯 C 组件和测试；
2. 运行全部测试二进制；
3. 扫描纯 C 源码，禁止出现 `esp_`、`freertos`、`hal/`、`sdkconfig` 等平台依赖；
4. 运行 ARD-1、PCA9685 安全包络、HUD、音频和 MP3 解码检查。

---

## 覆盖范围

| 测试 | 内容 |
| --- | --- |
| `test_leg_fsm.c` | Hold-to-Swing 全流程、超时/释放边界、配置校验、历史环回。 |
| `test_control_glue.c` | 只依赖可替换 capability、初始化校验。 |
| `test_ble_protocol.c` | 控制包校验矩阵、STATUS 布局、UUID 字节序。 |
| `test_remote_input_state.c` | 首包前不超时、精确 500 ms、断链清零、时钟回退。 |
| `test_ble_endpoint_mock.c` | Android 4 字节包 → 接收端 → STATUS/FAILSAFE 的端到端 mock。 |
| `test_pca9685_safety*.c` | 逐通道脉宽包络、静息值、拒绝放宽配置。 |
| `test_safety_contracts.c` | 所有失败组合都尝试三步 safe-stop、授权 epoch 拒绝旧帧。 |
| `test_pca9685_driver_stop.c` | 实际驱动的 `force_safe_stop` 路径和部分初始化。 |
| `test_display_hud_*.c` | 横竖布局、状态/颜色/爪子位置、非法输入保护。 |
| `test_voice_keyword.c` | 合成特征的模板匹配、静音、无模板、冷却。 |
| `test_audio_command.c` | `audio play/stop` 与 `NIULAI` 别名。 |
| `test_i2s_sine_command.c` | `tone/freq/amp/mute/status/help` 语法与范围。 |
| `test_mp3_decoder.c` | 同一 Helix 解码器解码 169 帧、44.1 kHz 双声道。 |
| `test_fall_detector.c` / `test_mpu6500_math.c` | 摔倒阈值、清除条件、角度/加速度换算。 |

> host suite 只证明纯逻辑和帧布局；不能证明 I²S 引脚、扬声器、屏幕方向、舵机力矩、电源或机械限位。

---

## HUD 预览

```bash
sh tools/hud_preview.sh /tmp/niulai-hud-preview
```

使用与固件相同的纯 framebuffer renderer，生成 `hud-portrait-*` 和 `hud-landscape-*` PPM 图片，不需要面板或开发板。

---

## 隔离舵机测试

`firmware-esp32/tools/servo-zero-test/` 是独立 ESP-IDF 工程，包含自己的 host 数学测试；它不接入正式 BLE/FSM/执行器路径。见 [`../../tools/servo-zero-test/README.md`](../../tools/servo-zero-test/README.md)。

`tests/on_target/` 保留给未来的 ESP-IDF Unity 测试。
