# 制造输出

本目录保存可提交给 PCB 厂商的制造输出，不保存原始模块照片或临时审阅截图。

- [`gerber/README.md`](gerber/README.md)：当前制造输出的唯一索引、SHA-256 和放行状态。
- [`gerber/`](gerber/)：当前 Carrier 原型 Gerber、钻孔和测试文件压缩包。

制造输出必须与对应的 Gerber review 记录和 SHA-256 一起交接。生成新版本时使用新的日期/版本文件名，并同步更新索引、评审记录和哈希；不要静默覆盖已有归档。

当前归档：

- [`gerber/Gerber_PCB1_2026-09-02.zip`](gerber/Gerber_PCB1_2026-09-02.zip)

> 制造放行 `PASS` 只代表该版本可以送去打样，不代表装配、供电、舵机负载或机械配合已经验收。
