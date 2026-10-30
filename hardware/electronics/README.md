# 电子硬件（electronics）

本目录保存牛来快跑 Carrier 的电源、接口、原理图、Gerber、BOM 和电气评审记录。

当前基线是 **Modular Carrier v0.2**：

- 单路 6 V XT30 输入；
- 外部 10 A ATO/ATC 保险；
- PCA9685 驱动 8 路舵机；
- LM2596S 生成 3.3 V；
- ESP32-S3 作为主控；
- MPU6500 / ST7789 通过通用 GPIO/电源 breakout 飞线接入；
- 无板上电源选择、无电子 ESTOP、无市电处理。

---

## 1. 权威入口

| 文件 | 内容 |
| --- | --- |
| [`MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md) | 电源架构、XT30、保险、舵机配电、GPIO breakout、机械约束和验收测试。 |
| [`MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv`](MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv) | 模块接口、候选 GPIO 和通用 breakout 表；带 `BLOCKED_*` 的行不是已冻结 pinout。 |
| [`MODULAR_CARRIER_POWER_PROTECTION_v0.1.md`](MODULAR_CARRIER_POWER_PROTECTION_v0.1.md) | 10 A 主保险、≥10 A 主路径、可选 1000–2200 µF / 16 V servo-bulk 的 DNP-first 策略。 |
| [`HARDWARE_PARAMETER_REGISTER_v0.1.md`](HARDWARE_PARAMETER_REGISTER_v0.1.md) | 尚未实测/未冻结参数登记表。**任何 TBD 都不能当成已验证值。** |
| [`MODULE_PINOUT_CONTRACT_v0.3.csv`](MODULE_PINOUT_CONTRACT_v0.3.csv) | 由实物照片推导的模块引脚顺序、置信度和 Carrier 网络契约。 |
| [`SCHEMATIC_INPUT_REQUEST_v0.2.md`](SCHEMATIC_INPUT_REQUEST_v0.2.md) | 可导入原理图前的模块证据清单。 |
| [`MODULE_PHOTO_EVIDENCE_v0.2.md`](MODULE_PHOTO_EVIDENCE_v0.2.md) | 模块照片逐张识别结果、可用引脚证据和不确定项。 |
| [`PCB_GERBER_REVIEW_2026-09-02.md`](PCB_GERBER_REVIEW_2026-09-02.md) | 当前 Gerber 的电气/装配评审和已接受偏差。 |
| [`PCB_MECHANICAL_FIT_REVIEW_2026-09-02.md`](PCB_MECHANICAL_FIT_REVIEW_2026-09-02.md) | 打板前机械包络复核。 |

---

## 2. 原理图与制造

- [`schematic/`](schematic/)：Carrier v0.2 Review Candidate 4，包含 KiCad 原理图、PDF/SVG、BOM、独立网络表、ERC 和嘉立创 EDA 导入包。
- [`manufacturing/gerber/`](manufacturing/gerber/)：当前制造 Gerber 的唯一索引、SHA-256 和放行状态。
- [`calculations/`](calculations/)：Carrier 接口与电源保护的纸面检查脚本。

重建原理图：

```bash
sh hardware/electronics/schematic/build_and_verify.sh
```

该脚本会重新生成原理图/CSV、调用 KiCad 升级、运行 ERC（违规即失败）、导出 PDF、运行独立设计契约检查并重建 ZIP。

---

## 3. 电源与安全要点

```text
External 6 V source
       │
       ▼
   J_PWR XT30
       │
 F_MAIN 10 A
       │
       ▼
  EXT_6V_IN
       │
       ├── SERVO_6V → servo distribution → 8× servo
       │       ├── C_BULK1 optional
       │       └── C_BULK2 optional
       └── LM2596S → 3V3 → logic/modules
```

- 只有**一个**主输入；禁止双电源、DC barrel、源选择/并联。
- `F_MAIN` 紧跟 XT30；未保护铜皮必须短而宽。
- `J_PWR`、保险座、`EXT_6V_IN`、`SERVO_6V` 和相关铜皮按 **≥10 A design class** 设计，但正常负载不声明为 10 A。
- PCA9685 `/OE` 通过 10 kΩ 上拉到 3V3；复位/断线默认禁止输出。
- **`/OE` 是动作禁止，不是电源隔离。** 必须能直接断开 XT30。
- 舵机 bulk 电容默认 DNP；按 0 µF → C_BULK1 → C_BULK2 的实测顺序决定。

---

## 4. 已知未冻结项

- ESP32 开发板的精确型号、排针方向和供电脚；
- 完整 GPIO 分配和未用 GPIO breakout 位置；
- XT30 板端公头的精确 MPN/封装；
- ATO/ATC 保险座 MPN/封装/keepout；
- PCA9685 的 Option A/B 配电选择；
- LM2596S、MAX98357A、MS4030 模块的精确接口/尺寸；
- MPU6500 / ST7789 的实物电压和引脚标签；
- KW10 的引脚配对、机械安装和最终结构；
- 板框、模块坐标和安装孔。

未完成上述项目之前，不得把候选 pinout 写成最终硬件规格。

---

## 5. 验收测试

- 分别测试 YSIDO、台式电源和隔离 6 V 适配器；核对 XT30 极性、6 V、线温和压降。
- 验证 `J_PWR → F_MAIN → EXT_6V_IN` 连续且不存在保险旁路。
- 在 ESP32 idle、显示、音频和舵机负载下测 3V3，确认无正常工作的 brownout。
- 从 1 路到 8 路逐级加载舵机，记录最低 `SERVO_6V`、UBEC/XT30/线束/PCB/模块温度和压降。
- 按 0 µF → C_BULK1 → C_BULK2 的顺序验证 bulk 电容。
- 验证上电/复位期间 `/OE` 保持高，无非受控脉冲/动作。
- 对每个足端开关做垂直/倾斜/重复触地测试，确认机械止挡保护开关。

实体舵机测试属于 **HIGH RISK**：必须机械固定、限流、人员净空、可立即断开 XT30。
