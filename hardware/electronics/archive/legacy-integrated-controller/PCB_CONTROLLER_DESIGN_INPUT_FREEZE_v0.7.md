# 牛来快跑 PCB Controller 设计输入冻结 v0.7

状态：**Superseded by v0.8 / 历史设计输入**

日期：2026-08-31

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.6.md)

## 1. 权威性与合并规则

本文曾是 PCB Controller Rev A Circuit Design 权威电气设计输入，并完整取代 v0.6；现已由 `PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.8.md` 取代。本文只保留 raw-3S、LM74800与板载SY8388历史设计，不得覆盖 v0.8。

本修订来自 Project maintainer 于 2026-08-31 的明确决定：保留所有 v0.6 功能和 `≤100 µA` inactive external power pin reverse-leakage要求，但不再要求由LM74800单颗datasheet提供不存在的整板同条件guaranteed maximum。Rev A首批每块PCB必须在25 °C与40 °C通过实际EOL反灌screen；后续批次保持逐板室温EOL并以40 °C批次抽检维持设计鉴定。旧冻结版本、BOM、Baseline、Decision、商城资料和聊天不得覆盖本文。

## 2. 不变的功能与数值要求

v0.6及其纳入的早期版本全部功能和安全语义保持不变，尤其包括：

- `ESP32-S3-WROOM-1-N16R8`及全部已冻结外设、接口和扩展能力；
- `SY8388ARHC / C5110279`板载6.0 V buck作为唯一正常6 V source，不恢复外置UBEC；
- physical fuse、active battery protection、MASTER/NC ESTOP、source isolation和default-safe；
- 任一断开或未使能的外部电源端口不得被其他source有意供电，不得形成charging path；
- source pin short-to-GND时，reverse blocker必须阻止另一source持续向该短路供能；
- 正常温度下inactive external power pin从板内流向外部的steady reverse leakage仍须`≤100 µA`。

本次只修订最后一项的证据和量产验收规则，不放宽数值、不删除保护器件、不降低故障隔离能力。

## 3. 反灌保证的证据规则

`≤100 µA`是**整板端口要求**，其证明必须覆盖实际MOSFET、controller、TVS、分压/保护网络、ESD器件、焊接残留、PCB表面和测量不确定度。LM74800关于zero DC reverse current的功能描述只能支持拓扑选择，不能单独证明整板数值。

Circuit Design必须建立可审阅的leakage budget，至少逐项列出：

```text
I_BACKFEED =
    external back-to-back MOSFET off-state leakage
  + LM74800 A/C/VS及关断相关路径
  + TVS / ESD / connector protection leakage
  + 跨隔离边界的UV/OV/enable/sense网络电流
  + PCB表面污染、fixture和仪表不确定度
```

只有具备匹配电压、温度和状态的manufacturer maximum才能作为纸面上界；typical值、不同reverse-battery条件或EVM波形只能作为设计参考。没有可用maximum的贡献项必须由整板qualification/EOL实测覆盖，不能填零或以功能描述代替。

## 4. 首批每板 EOL 硬门槛

“首批”指Rev A首次组装批次中全部拟继续bring-up、交付或用于实体实验的PCB assembly，包括返修后重新进入使用的板。每块板必须具有唯一序列号或可追溯标识，并保存原始测量记录。

每块首批PCB必须在清洁、干燥、目检合格后执行25 °C与40 °C EOL反灌screen；不允许以抽检替代：

- 环境温度：室温点`25±5 °C`、高温点`40±3 °C`，均记录实测值并在板达到热稳态后测量；
- USB POWER、USB DATA和电池等外部source按test matrix逐项连接或断开，不允许通过未记录的fixture路径供电；
- 覆盖`MASTER_ENABLE=OFF`与`ON`、LM74800 disabled与enabled可达状态；
- 分开覆盖电池端被0 V低阻测量端接，以及由精密电流计/SMU构成的高阻开路等效；物理开路本身不能作为电流测量；
- 对LM74800专项，在`VBAT_RAW`侧与`VBAT_PROTECTED/SERVO_BUCK_IN`侧施加明确电压，覆盖`VBAT=9.0 V/12.6 V`、受保护侧`VBAT+0.25 V`的小差压，以及`VBAT_RAW=0 V`时受保护侧`9.0 V/12.6 V`；不得把`BUCK_6V`或`SERVO_V+`的回生测试混入该数值；
- `MASTER`与LM74800 enable组合必须标为正常可达状态或fixture强制故障状态，不能把不可达组合伪装成普通工作状态；
- 分别测量每个受影响external power pin的`I_PORT`，并记录整板`I_TOTAL`；不得用总电流掩盖单个端口超限；
- 必须区分电容充电瞬态与稳态，ramp rate、等待时间、稳态判据、fixture guarding、burden voltage和放电步骤写入test spec；
- 仪表、fixture和计算给出的95%扩展不确定度记为`U95`。

首批EOL初始放行门槛为：

```text
|I_PORT| + U95 ≤ 50 µA  at 25±5 °C
|I_PORT| + U95 ≤ 75 µA  at 40±3 °C
```

50/75 µA是为批次、老化和污染留出的工程guardband，不替代100 µA系统要求；任何测点在任何冻结状态下均不得超过`100 µA`最终功能限值。任何状态超出对应guardband即整板FAIL：必须隔离、保留原始失败记录并返修或报废；返修后必须重跑本节全部状态。不得通过固件校准、临时跳线、跳过状态、取平均掩盖超限或扩大限值放行。

## 5. 设计鉴定、批次抽检与40 °C要求

首批逐板25/40 °C EOL之外，还必须完成设计鉴定；40 °C是v0.3冻结的最大局部环境温度。设计鉴定不得代替逐板EOL。

```text
|I_PORT| + U95 ≤ 100 µA
```

qualification还须包含：

- 慢速OUT>BAT变化与快速断开/恢复分别测试；
- MASTER/EN状态切换前后的稳态和过渡波形；
- 清洗前后或已定义的最终制造清洁状态；
- 至少覆盖首批中实际使用的器件lot/assembly process，并记录样本数量；首批少于3块时全部进入设计鉴定，否则至少3块且覆盖不同panel/回流位置；
- 至少100次MASTER cycle、100次ESTOP latch/clear和100次INA300 fault latch/clear后重跑40 °C矩阵；
- 70 °C作为非工作加速裕量点重跑代表状态，但不能替代25/40 °C pass，也不能据此扩大正常工作限值。

后续批次至少执行与首批相同的每板室温EOL screen，直至新的、经人类review接受的production control plan明确取代；任何抽检缩减不得由agent自行决定。40 °C按批次抽检，抽检数量、lot定义和触发全批复测的规则必须在production control plan中冻结。

## 6. 测试可实施性要求

Circuit Design必须提供不会成为用户外部供电接口的bed-of-nails测试点，使fixture能够独立接触battery connector侧、`VBAT_PROTECTED/SERVO_BUCK_IN`侧、LM74800 enable状态和测量参考地。测试点不得绕过physical fuse、ESTOP或正常保护路径供用户使用。

EOL/qualification test specification必须冻结：接线图、SMU/电源四线连接、compliance、ramp rate、settling/steady判据、放电步骤、环境条件、仪表分辨率/校准、空fixture绝缘/归零、`U95`计算、状态矩阵、原始signed current和`I_PORT/I_TOTAL`数据格式、序列号、硬件/BOM版本、器件lot、fixture/仪器校准状态、操作者、FAIL隔离和返修重测。`U95`至少合成仪表校准、量程分辨率、fixture寄生、重复性和供电误差，测量burden不得改变LM74800/FET状态。未定义这些内容时，不得宣称首批每板获得实际保证。

## 7. 对 CDB-005 与 EDD-002 的影响

本修订接受原CDB-005 closure option C：不再等待LM74800提供整板同条件guaranteed maximum。CDB-005可由本Design Input修订关闭；缺少的整板数据转为active prototype/production validation risk，并阻塞Circuit Design Freeze candidate，直到leakage budget、test specification和fixture-access要求完整。

`LM74800QDRRRQ1 + external back-to-back MOSFET`仍只是EDD-002 preferred candidate，不因本修订自动成为Accepted或证明合格。exact MOSFET高温漏电、TVS、跨域网络、default/fault states和实测结果仍须分别审阅。

## 8. 阶段与证据边界

本修订不关闭DIB-013 TVS系统瞬态包络，不选择新的exact MPN，也不授权schematic、ERC、netlist、PCB layout、采购、下单或实体测试。下一步属于Circuit Design：建立leakage budget/state matrix/EOL test spec，关闭DIB-013，并继续完成exact component、pin/default/fault tables和可复现计算。
