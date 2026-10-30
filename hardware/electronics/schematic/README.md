# Carrier v0.2 原理图包

状态：**Review Candidate 4 — 模块级 pinout 候选，已用于原型打样评审**

本目录包含牛来快跑 Modular Carrier v0.2 的 KiCad 原理图、评审渲染、BOM、网络表和导入包。

## 主要文件

| 文件 | 内容 |
| --- | --- |
| `niulai_carrier_v0.2.kicad_sch` | KiCad 10 原理图（内嵌项目符号） |
| `niulai_carrier_v0.2.kicad_pro` | KiCad 项目元数据 |
| `niulai_carrier_v0.2.kicad_sym` / `sym-lib-table` | 项目本地符号库 |
| `fp-lib-table` | KiCad footprint 库引用；`FOOTPRINT_TBD` 仍未绑定 |
| `niulai_carrier_v0.2.pdf` / `svg/` | 评审渲染 |
| `niulai_carrier_v0.2_bom.csv` | 采购/封装绑定草稿 |
| `niulai_carrier_v0.2_netlist.csv` | 独立 pin-to-net 参考 |
| `../MODULE_PINOUT_CONTRACT_v0.3.csv` | 照片推导的物理引脚顺序与网络契约 |
| `niulai_carrier_v0.2.net` | KiCad XML 网络表，供 EasyEDA Pro 交叉检查/导入 |
| `niulai_carrier_v0.2_erc.rpt` | KiCad ERC 结果 |
| `niulai_carrier_v0.2_easyeda_import.zip` | EasyEDA Pro 导入/评审包 |

## 重建

```bash
sh hardware/electronics/schematic/build_and_verify.sh
```

脚本会重新生成原理图/CSV、调用已安装 KiCad 升级、运行 ERC（违规即失败）、导出 PDF、运行独立设计契约检查并重建 ZIP。

## 设计要点

- 一个完整的 `U1` ESP32 2×22 模块；`L01..L22`、`R01..R22` 按卖家图片从天线端到 USB 端编号。
- PCA9685、MAX98357A、MS4030、LM2596S 以完整模块表示，而不是分散的排针片段。
- 每个已连接引脚都有显式可见导线段接到命名网络；电源/地使用命名网络，避免不安全的多点飞线。
- PCA9685 符号包含六针控制排针、绿色端子极性和 CH0–CH7 的 `PWM/V+/GND` 输出。
- 原理图转换正确性不能由格式转换本身证明；必须逐针比对网络表和 pinout 契约。

## EasyEDA Pro 交接

1. 将 `niulai_carrier_v0.2_easyeda_import.zip` 作为 KiCad 工程导入。
2. 确认 ESP32 作为单个 `U1` 导入，并逐针对照 `niulai_carrier_v0.2_netlist.csv` 和 `MODULE_PINOUT_CONTRACT_v0.3.csv`。
3. 为所有空白/TBD footprint 绑定 EasyEDA/LCSC 器件或经验证的自定义封装。
4. 确认 C1/C2、R2/R3、C5–C8 的 DNP 状态。
5. 导入后运行 EasyEDA 原理图 DRC。
6. 在所有精确机械封装和物理模块 pinout 关闭前，不要开始最终 PCB 布线。

## 保留假设

- ESP32 板身份和排针映射来自卖家图片，不是实际板连续性测量。
- J3/J4 pin 1 在天线端、pin 22 在 USB 端，需在 footprint release 前对照实物。
- GPIO35–37 保留给 N16R8 octal PSRAM；GPIO19/20 为 USB；GPIO0/3/45/46 为 strapping。
- GPIO38/48 的板载 RGB/功能分配需实物确认。
- PCA9685 Option A 承担 servo V+；电流/温度/压降仍需资格测试。
- MPU6500 和 ST7789 通过 J5/J6 杜邦飞线，没有专用 Carrier footprint。
- LM2596S 输入 3.2–40 V、输出 1.25–35 V、要求至少 1.5 V 压差；6.0→3.3 V 的 2.7 V 压差纸面通过，但启动、负载调整率、纹波和温度仍需实测。

> 格式转换、ERC 和纸面检查都不能替代实物连续性、装配和满载测试。
