# Carrier Gerber 归档

这里是仓库中唯一的 Carrier PCB 制造输出目录。原始模块照片、临时截图和历史集成板资料不放在这里。

## 当前文件

[`Gerber_PCB1_2026-09-02.zip`](Gerber_PCB1_2026-09-02.zip)

- SHA-256：`3776462b60997e3d55e2a6b77d7f8bb6cecb333b17b7e10cbf4b5b7e490bc87c`
- 完整性检查：

```bash
unzip -t hardware/electronics/manufacturing/gerber/Gerber_PCB1_2026-09-02.zip
```

## 状态

该 ZIP 是 2026-09-02 原型 Gerber，已做电气/机械评审并标记为制造放行 `PASS`。以下风险仍然保留：

- PCA9685 / MAX98357A 竖装高度；
- 车体净空和第二固定点；
- ESP32 天线附近机械条件；
- 板上未装 bulk 电容、NPTH 安装孔周围的隔离铜环等已接受偏差。

权威结论见：

- [`../../PCB_GERBER_REVIEW_2026-09-02.md`](../../PCB_GERBER_REVIEW_2026-09-02.md)
- [`../../PCB_MECHANICAL_FIT_REVIEW_2026-09-02.md`](../../PCB_MECHANICAL_FIT_REVIEW_2026-09-02.md)

生成新的制造输出时，必须使用新的日期/版本文件名，并同步更新本索引、上级制造 README、Gerber review 和 SHA-256；不要静默覆盖当前归档文件。
