# Third-Party Notices

牛来快跑的原创代码采用 MIT License。以下组件、工具或素材由第三方提供，适用各自许可证；本文件不改变这些许可证的效力。

## 1. Helix MP3 Decoder

- 位置：`firmware-esp32/components/mp3_decoder/`
- 来源：Espressif `esp-dev-kits` 的 Kaluga 示例中引入的 Helix 定点 MP3 解码器。
- 许可证：**RCSL 1.0 / RPSL 1.0**（RealNetworks Community Source License / RealNetworks Public Source License）。
- 每个导入的源文件保留原始版权与许可证头。使用、复制、修改或再分发前，请阅读文件头中的完整条款，并在需要时向 RealNetworks 取得适用许可。
- 该组件被隔离在独立 component 边界内，便于替换为其他已获授权的解码器。

## 2. Android 与 Kotlin 依赖

`android-controller/` 使用 AndroidX、Jetpack Compose、Kotlin、Kotlin Coroutines、Gradle 等组件。它们通常采用 Apache License 2.0 或各自的开源许可证；具体版本和条款见 `android-controller/app/build.gradle.kts`、Gradle wrapper 以及各依赖的发布页面。

## 3. ESP-IDF / ESP-NimBLE

ESP-IDF 与 ESP-NimBLE 采用 Apache License 2.0。固件仓库不复制完整的 ESP-IDF 源码，而是通过 `idf.py` 和组件依赖使用已安装的 SDK。

## 4. 硬件工具与文档

- KiCad 原理图/PCB 文件由 KiCad 生成；KiCad 采用 GPL 相关许可证，但本仓库只包含设计文件，不包含 KiCad 程序本身。
- `hardware/electronics/archive/` 中可能包含第三方模块资料、参考手册或厂商文件；这些文件的权利归各自权利人，引用不代表重新授权。

## 5. 界面字体与图像

`android-controller/app/src/main/res/font/niulai_font.ttf` 以及 `drawable-nodpi/` 中的界面图片由项目方提供或从公开素材整理。仓库不额外声明这些素材的再许可；公开分发或商业使用前，请确认字体、插画和照片的授权范围。

## 6. 演示音频

- 文件：`牛来叫妈妈.mp3`
- 用途：仅作为固件音频链路和游戏反馈的演示素材。
- 权利：归原权利人所有。仓库不声明拥有该音频的版权。公开分发、商业使用或再分发前，请自行确认授权，或替换为你有权使用的素材。

---

如果你认为本仓库中的某个文件侵犯了你的权利，请通过 issue 联系维护者，我们会尽快处理。
