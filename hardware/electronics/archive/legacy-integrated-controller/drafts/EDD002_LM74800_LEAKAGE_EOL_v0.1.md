# EDD-002 LM74800 整板反灌预算与首批逐板 EOL 规范 v0.1

状态：**Circuit Design draft / v0.7执行规范；没有实测结果，不是EDD-002 acceptance或schematic authorization**

日期：2026-08-31

Owner：Project maintainer

权威输入：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md`](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md)

关联台账：[`PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md`](../PCB_CONTROLLER_CIRCUIT_DESIGN_OPEN_ISSUES_v0.1.md)

## 1. 目的与保证边界

本规范把原CDB-005从“等待LM74800单颗datasheet给出整板maximum”改为“纸面预算 + 首批每板25/40 °C EOL + 设计鉴定”。功能要求不变：每个inactive external power pin分别满足steady reverse leakage `≤100 µA`，且不得形成charging path。

合格措辞仅允许为：

> 该序列号PCBA在记录的EOL日期、fixture/instrument revision、9.0/12.6 V、25/40 °C和规定状态矩阵下，实测满足本规范guardband及`≤100 µA`最终限值。

不得把该结果扩大为未测温度、凝露、污染、老化、改板或替代料条件下的无限期datasheet guarantee。

## 2. 已核实的器件事实与不能外推的内容

| Contributor | 可用于paper audit的manufacturer maximum | 不能据此声称 |
| --- | --- | --- |
| `HD303N080SG ×2` | 单颗`IDSS ≤1.0 µA @ VDS=30 V, VGS=0 V, TJ=25 °C`；`abs(IGSS)≤100 nA @ abs(VGS)=20 V, TJ=25 °C` | 没有40/70 °C `IDSS` maximum；两个单管值不能自动成为整板端口maximum |
| `LM74800QDRRRQ1` | shutdown和各pin current只在各自datasheet条件下使用 | `I(C)` reverse-battery条件、`I(REV)`反接条件以及“zero DC reverse current”功能描述均不等于本项目正常正输入OUT→BAT整板maximum |
| TVS、ESD、BAS116、TPS70933、INA300、分压/kill网络 | exact MPN、方向、温度和电压匹配时才允许采用maximum | 未取得或条件不匹配时不得填零；必须进入整板实测 |
| PCBA/fixture | 无单颗datasheet bound | 助焊剂残留、湿度、connector污染、pogo/线缆绝缘和burden voltage必须由工艺及测量覆盖 |

HD303的室温条件化筛选和为：

```text
2 × IDSS,max + 2 × |IGSS|max = 2 × 1.0 µA + 2 × 0.1 µA = 2.2 µA
```

该`2.2 µA`只证明MOSFET datasheet中四个独立25 °C最大项的保守和，既不是两只串联器件真实漏电的电路模型，也不是整板PASS结论。

## 3. Leakage budget模板

每次BOM或网络修订必须更新下表；`TBD/test`不允许被当成0。

| Path / contributor | 25 °C paper bound | 40 °C paper bound | Evidence/condition | Closure |
| --- | ---: | ---: | --- | --- |
| QDIODE/QLOAD drain+gate条件化和 | `2.2 µA` | `TBD/test` | HD303 Rev.1.0，25 °C独立maximum的保守和 | 25 °C局部bound；高温逐板实测 |
| LM74800 A/C/VS/DGATE/HGATE | `TBD/test` | `TBD/test` | 没有匹配OUT>BAT整板maximum | 逐板实测 |
| AON LDO/BAS116/INA300/wired-kill跨域 | `TBD` | `TBD` | 等待final connection与exact maximum | 纸面审计 + 实测 |
| Battery-entry TVS/ESD | `TBD` | `TBD` | 等待DIB-013与exact MPN | 纸面审计 + 实测 |
| UV/OV/EN/sense/passive跨域 | `TBD` | `TBD` | 等待pin/net table | 计算 + 实测 |
| PCB表面、connector、fixture | test-only | test-only | 最终清洗和fixture状态 | blank fixture + 每板实测 |
| Measurement expanded uncertainty | `U95≤5 µA` target | `U95≤5 µA` target | calibration + repeatability + fixture + source error | 每次run记录 |

最终审核必须同时保留每个端口的`I_PORT`和整板`I_TOTAL`。某一端口超限不能被另一路流入电流抵消。

## 4. Fixture与仪器

- 两台四象限SMU或等效source/sink measurement system，关键端口四线连接；
- current resolution `≤0.1 µA`，目标`U95≤5 µA`；
- 每次run前进行open/short/blank-fixture检查，blank leakage `≤2 µA`；
- 每路compliance `≤25 mA`；达到compliance立即FAIL并停止该bias step；该测试不替代高能source-short保护测试；
- pogo只接触生产测试焊盘：`VBAT_RAW`、`VBAT_PROTECTED/SERVO_BUCK_IN`、GND、LM enable/状态观察点；这些焊盘不得作为用户注入接口；
- USB、servo、display和其他外部connector保持test matrix规定状态，fixture不得形成隐藏供电或接地回路；
- 电压ramp、burden和强制状态不得超过器件额定或改变被测MOSFET的预期on/off状态。

`U95`至少合成instrument calibration、range/resolution、source voltage error、fixture residual、重复性和温度测量误差。不得只采用仪表显示分辨率。

## 5. 电流定义

定义`I_PORT>0`为电流从PCBA内部流向被测外部端口。对`VBAT_RAW`高阻开路等效，由SMU/跨阻输入形成闭合测量回路；物理断开只允许观察电压，不能作为电流结论。

正常正输入、小差压状态可能同时存在AON正常耗流。为避免正常耗流掩盖反灌，除直接记录signed `I_PORT`外，还记录差分：

```text
I_BF,DIFF = I_BAT,sink(OUT = VBAT + 0.25 V)
          - I_BAT,sink(OUT = high-Z baseline)
```

正负号、source/sink方向和baseline必须在fixture software中用已知电流样件验证；既要检查`|I_PORT|`门槛，也要保存`I_BF,DIFF`，不得只保存相减后的结果。

## 6. 首批每板状态矩阵

每块首批PCBA在`25±5 °C`和`40±3 °C`均执行完整矩阵。若某组合在最终hardware truth table中不可正常到达，必须标记`FIXTURE-FORCED FAULT STATE`，并记录强制点；不得删项或冒充正常状态。

| ID | `VBAT_RAW` | `VBAT_PROTECTED/SERVO_BUCK_IN` | Hardware state | Measurement purpose |
| --- | ---: | ---: | --- | --- |
| N1 | `9.0 V` | `9.25 V` | MASTER ON、ESTOP closed、INA300 no-fault、LM enabled | 正常正输入小差压反灌 |
| N2 | `12.6 V` | `12.85 V` | 同N1；12.85 V只施加于隔离fixture节点并须先完成额定审计 | 高电压端小差压反灌 |
| O1 | `9.0 V` | `9.25 V` | MASTER OFF、LM disabled | off-state反灌 |
| O2 | `12.6 V` | `12.85 V` | 同O1 | off-state高端反灌 |
| E1/E2 | `9.0/12.6 V` | `VBAT+0.25 V` | ESTOP hardware latch分别强制 | ESTOP关断路径 |
| F1/F2 | `9.0/12.6 V` | `VBAT+0.25 V` | INA300 ALERT latch分别强制 | overcurrent关断路径 |
| Z1 | `0 V` sink | `9.0 V` | MASTER OFF；随后MASTER ON可达状态 | battery pin 0 V隔离 |
| Z2 | `0 V` sink | `12.6 V` | 同Z1 | 最大正常保护侧电压下0 V隔离 |
| H1/H2 | high-Z current measurement | `9.0/12.6 V` | MASTER OFF；随后MASTER ON可达状态 | 开路等效端口漏电与浮升 |

N/O/E/F状态都须同时取得对应OUT high-Z baseline。USB POWER未接、USB DATA未接作为LM74800专项默认；USB相关端口必须在独立system reverse-leakage matrix中逐口验证，不能用本表替代。

## 7. Sequencing、稳态与判定

每个bias step：

1. 从0 V开始，按test spec冻结的ramp rate上升；
2. 检查compliance、器件状态和节点电压；异常立即停止；
3. 等待至少60 s，随后采样10 s；若10 s内漂移`>1 µA`则延长，仍不稳定即FAIL；
4. 保存raw signed current、四线电压、温度、硬件状态和时间序列；
5. 按规定次序回落到0 V并主动放电，确认安全后进入下一状态。

逐板放行要求：

```text
25±5 °C: |I_PORT| + U95 ≤ 50 µA
40±3 °C: |I_PORT| + U95 ≤ 75 µA
all states: |I_PORT| + U95 ≤ 100 µA functional ceiling
```

达到compliance、不能稳定、状态/节点失控或guardband超限均为FAIL。FAIL板隔离并保留首测数据；清洗、返修或换件后必须重跑两温度完整矩阵，不能只复测失败点。返修板不计入首测良率。

## 8. 设计鉴定与后续批次

首批少于3块时全部参加；否则至少3块且覆盖不同panel/回流位置：

- 70±3 °C非工作加速裕量点复测N/O/E/F/Z代表状态；仍不得超过100 µA，但该点不扩展正常工作温度；
- 100次MASTER OFF→ON、100次ESTOP latch/clear、100次INA300 fault latch/clear后，重跑40 °C完整矩阵；
- 最终清洗工艺前后或工艺确认样件比较；
- BOM、PCB表面处理、清洗剂、conformal coating、LM74800/MOSFET/TVS/LDO或跨域网络改变后重新鉴定。

后续批次保持每板25 °C EOL；40 °C按经人类review接受的production control plan抽检。在该plan形成前，不得由agent自行减少测试。

## 9. Traceability记录

每板至少保存：serial、PCB/assembly/BOM revision、关键器件lot/date code、fixture/software revision、仪器ID与calibration due date、operator、温度、每个matrix ID的raw data、`U95`、判定、失败root cause、返修内容/次数和完整重测结果。

## 10. 尚未闭合

- exact LM74800周边连接和正常/强制状态truth table尚未完成；
- TPS709/BAS116/INA300/TVS/分压/kill网络的跨域maximum尚未填入budget；
- DIB-013未关闭，battery-entry TVS仍不能最终冻结；
- USB POWER、USB DATA和servo-source isolation需各自的system port reverse-leakage matrix；
- 本文没有实体测量结果，PVR-017保持active并阻塞首批放行。
