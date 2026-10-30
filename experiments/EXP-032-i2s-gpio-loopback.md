# EXP-032 ESP32 I²S GPIO4→GPIO14 电气回环

## Objective

用已验证的 I²S TX 输出代替 MS4030，直接把 GPIO4 接到 GPIO14，确认 ESP32-S3 的
TX、物理导线和 RX 能否在同一组 BCLK/WS 下收发可解析的数字音频样本。

## Hypothesis

如果 GPIO4→GPIO14 的直接回环能收到持续变化且同时包含正负值的 32-bit 样本，则
ESP32 I²S 初始化、时钟、GPIO 回环和 RX slot 解析链路基本正常；此前 MS4030 采集的
全零/稀疏单极性结果应继续从麦克风模块、SD/LR 接线或其输出时序排查。

## Hardware version

- ESP32-S3 QFN56 rev v0.2，8 MB PSRAM，16 MB flash；宿主机 Windows 原生 CH343
  串口 `COM4`。
- Operator 将 `GPIO4` 直接飞线连接到 `GPIO14`；GPIO9/10 仍作为 I²S BCLK/WS。
- 测试 profile 不启动 BLE、FSM、PCA9685 或舵机，`/OE` 保持安全状态。

## Software commit

`674d9a5-dirty`（共享工作区未提交 exploratory work）。

## Parameters

- ESP-IDF v5.5.5，ESP32-S3 target。
- Philips I²S，16 kHz，32-bit data，32-bit physical slot，stereo/both slot。
- TX=`GPIO4`，RX=`GPIO14`，BCLK=`GPIO9`，WS/LRC=`GPIO10`。
- TX 输出 1 kHz、25% 数字幅度的正弦波；发送 64 个 256-frame block，约 1.024 秒。
- UART 输出前 32 个 RX frame，并统计总 word 数、非零、正负值、最小值和最大值。

## Test method

1. 使用独立 `/tmp/niulai-i2s-loopback` build 目录和临时 defaults 开启
   `CONFIG_NIULAI_I2S_LOOPBACK_TEST=y`，并固定 GPIO4/14/9/10。
2. 使用 ESP-IDF v5.5.5 环境构建：

   ```bash
   env IDF_PATH=/tmp/niulai-esp-idf-v5.5.5 \
     IDF_TOOLS_PATH=/tmp/niulai-idf-tools \
     IDF_PYTHON_ENV_PATH=/tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env \
     ESP_ROM_ELF_DIR=/tmp/niulai-idf-tools/tools/esp-rom-elfs/20241011 \
     PATH=/tmp/niulai-idf-tools/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin:$PATH \
     cmake -S firmware-esp32 -B /tmp/niulai-i2s-loopback -G Ninja \
     -DIDF_TARGET=esp32s3 \
     -DPYTHON=/tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env/bin/python \
     -DPYTHON_DEPS_CHECKED=1 \
     -DSDKCONFIG=/tmp/niulai-i2s-loopback/sdkconfig \
     -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;/tmp/niulai-i2s-loopback.defaults'
   env IDF_PATH=/tmp/niulai-esp-idf-v5.5.5 \
     IDF_TOOLS_PATH=/tmp/niulai-idf-tools \
     IDF_PYTHON_ENV_PATH=/tmp/niulai-idf-tools/python_env/idf5.5_py3.14_env \
     ESP_ROM_ELF_DIR=/tmp/niulai-idf-tools/tools/esp-rom-elfs/20241011 \
     PATH=/tmp/niulai-idf-tools/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin:$PATH \
     cmake --build /tmp/niulai-i2s-loopback --parallel 4
   ```

3. 从生成的 `flasher_args.json` 核对地址和参数后，通过宿主机 PowerShell/native
   esptool 4.12.0 烧录 `COM4`；未执行全片擦除：

   ```powershell
   Set-Location "\\wsl.localhost\牛来快跑\tmp\niulai-i2s-loopback"
   python -m esptool --chip esp32s3 --port COM4 --baud 460800 `
     --before default_reset --after hard_reset write_flash `
     --flash_mode dio --flash_size 16MB --flash_freq 80m `
     0x0 bootloader/bootloader.bin 0x10000 niulai.bin `
     0x8000 partition_table/partition-table.bin
   ```

4. 复位并读取 8 秒 UART 日志，等待 profile 自动完成一次回环。

## Video / logs

没有保存原始波形或音频文件；完整烧录和串口输出位于本会话工具日志中。烧录时
bootloader、app、partition table 均报告 `Hash of data verified`。

## Result

- ESP-IDF 构建通过，`niulai.bin` 为 `0x3e7c0`（255936 bytes），应用分区余量
  76%。生成配置确认 `LOOPBACK_TEST=1`、`BCLK=9`、`WS=10`、`TX=4`、`RX=14`。
- 启动日志确认：

  ```text
  I2S_LOOPBACK_CONFIG sample_rate_hz=16000 bclk_hz=1024000 format=philips data_bits=32 slot_bits=32 slot_mode=stereo slot_mask=both tx_gpio=4 rx_gpio=14 bclk_gpio=9 ws_gpio=10 tone_hz=1000 amplitude_percent=25
  ```

- 前 32 个打印 frame 为零值，但完整统计为：

  ```text
  I2S_LOOPBACK_RESULT tx_gpio=4 rx_gpio=14 blocks=64 rx_words=32768 nonzero=25720 negative=12866 positive=12854 min=-536870911 max=536870911
  ```

- 结论：**GPIO4→GPIO14 I²S 数字回环通过。** RX 在完整测试期间收到了持续变化的
  正负样本，且幅度与 25% 的 32-bit TX 正弦波一致。前 32 frame 的零值只能说明
  读取起始时刻可能处于 DMA 初始/空数据阶段，不能推翻后续统计。
- 本实验只证明 ESP32 数字链路，不证明 MAX98357A 的听感，也不证明 MS4030 的 SD
  输出正常。当前板卡仍运行该回环 profile；恢复麦克风测试前必须拆除 GPIO4→GPIO14
  飞线，并将 MS4030 SD 接回 GPIO14。

## Next step

拆除回环线后恢复 MS4030 接线，固定 L/R 电平，再用同一 16 kHz/32-bit 配置重新读取
`voice raw`，分别检查 active slot 和 GPIO14 的实际数据。
