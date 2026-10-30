# MP3 解码器组件

本组件包含音频播放使用的 **Helix 定点 MP3 解码器**。C 源码和头文件来自 Espressif `esp-dev-kits` 的 Kaluga 示例：

```text
examples/esp32-s2-kaluga-1/components/helix/
```

## 许可证

每个导入的源文件保留原始 **RCSL 1.0 / RPSL 1.0** 版权与许可证头。使用、复制、修改或再分发前，请阅读文件头中的完整条款，并在需要时向 RealNetworks 取得适用许可。

本组件被隔离在独立 component 边界内，便于替换为其他已获授权的解码器，而不改变播放器命令/API。

## 边界

- 只负责 MP3 → PCM 解码；
- 不访问 I²S、BLE、FSM 或执行器；
- 播放器从 MP3 首帧读取采样率/声道；
- 当前演示资源为 44.1 kHz 立体声，输出候选为 32-bit stereo；
- 构建/解码成功不等于扬声器有声，实际听测仍需 MAX98357A 台架验收。

完整第三方声明见仓库根目录 [`THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md)。
