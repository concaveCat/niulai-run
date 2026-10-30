# TFT HUD 资源

三个 `.rgb565` 文件是 320×170、little-endian RGB565 framebuffer：

| 文件 | 场景 |
| --- | --- |
| `waiting_for_ble.rgb565` | 等待连接 |
| `walking_solo.rgb565` | 单人 |
| `walking_multi.rgb565` | 双人 |

固件会在 ARD-1 快照处于 `LIFT/SWING/SEARCH` 的腿对应角落叠加黑色爪子图标；背景本身对四条腿通用。

## 转换

每像素：

```text
(r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
```

先转成 depth-8 RGB 字节，再按 little-endian 16-bit 写入。

这些资源只用于显示，不参与控制、安全门或执行器授权。
