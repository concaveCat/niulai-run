# 硬件

这里保存牛来快跑的电子、机械和原型记录。硬件仍在原型阶段：能制造/能点亮不等于已完成装配、标定和安全验收。

```text
hardware/
├── electronics/     Carrier v0.2、原理图、Gerber、BOM、电气评审
├── mechanical/      CAD、尺寸、机构实验入口
└── prototypes/      短期实体原型、照片/视频和对应实验
```

## 当前基线

| 项目 | 内容 |
| --- | --- |
| 控制器 | ESP32-S3-WROOM-1-N16R8 开发板 |
| 执行器 | 8× MG90S 级舵机，PCA9685 50 Hz PWM |
| 电源 | 单路 6 V XT30 输入，外部 10 A ATO/ATC 保险，LM2596S 3.3 V |
| 足端 | 4× KW10 C-NC |
| 姿态 | MPU6500（I²C，飞线） |
| 显示 | ST7789 170×320 / 320×170（SPI，飞线） |
| 音频 | MAX98357A + 扬声器；MS4030 麦克风（可选诊断） |
| PCB | 2 层 FR-4 Modular Carrier v0.2 |

## 入口

- [`electronics/README.md`](electronics/README.md) — 电子资料总入口。
- [`electronics/MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md`](electronics/MODULAR_CARRIER_HARDWARE_SPEC_v0.2.md) — 电源、接口、保险和机械约束。
- [`electronics/HARDWARE_PARAMETER_REGISTER_v0.1.md`](electronics/HARDWARE_PARAMETER_REGISTER_v0.1.md) — 未实测/未冻结参数登记表。
- [`electronics/schematic/`](electronics/schematic/) — KiCad 原理图、PDF/SVG、BOM、网络表、ERC。
- [`electronics/manufacturing/gerber/`](electronics/manufacturing/gerber/) — 制造 Gerber 和放行记录。
- [`mechanical/README.md`](mechanical/README.md) — 机械记录。
- [`prototypes/README.md`](prototypes/README.md) — 短期原型记录。

## 安全

> `/OE` 是动作禁止，不是电源切断。任何时候都必须能够直接断开 XT30 主电源。

- 舵机主回路必须串入规定的 10 A ATO/ATC 保险。
- 首次上电不装舵机臂/连杆；先核对 GND/V+/PWM 方向和 PCA9685 ACK。
- 6 V 电源必须限流；多舵机同时动作时记录电压、电流、温升和 I²C 错误。
- 机械固定、人员净空、急停可达后再做实体动作。
- 任何未实测参数都必须保留 `TBD`，不能用“通用默认值”冒充标定结果。

详细流程见根目录 [`README.md`](../README.md#安全边界)、[`../docs/I2C_SERVO_TROUBLESHOOTING_GUIDE.md`](../docs/I2C_SERVO_TROUBLESHOOTING_GUIDE.md) 和 [`../docs/NACK_POWER_DIAGNOSIS_CASE_STUDY.md`](../docs/NACK_POWER_DIAGNOSIS_CASE_STUDY.md)。
