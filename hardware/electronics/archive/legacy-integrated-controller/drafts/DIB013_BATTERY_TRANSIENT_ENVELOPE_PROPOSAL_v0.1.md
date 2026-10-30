# DIB-013 电池入口有限能量瞬态包络提案 v0.1

状态：**Proposed Design Input；第1项已由Operator确认，第2–4项待确认；不关闭DIB-013，不授权schematic/layout/实体注入测试**

日期：2026-08-31

Owner：Project maintainer

权威输入：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md`](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md)

## 1. 结论与职责分界

Rev A battery-entry TVS只负责可拆卸3S LiPo和有限线束产生的热插拔/断流瞬态，不承担汽车load-dump，也不承担`SERVO_V+`舵机回生能量。低成本闭合方法是冻结有限能量qualification波形并验证实际布局寄生，而不是按任意高能外部source堆大TVS。

舵机回生是独立问题：`BUCK_SERVO_ISO`在off时必须双向阻断；在证明SY8388/电池路径可安全吸收前，默认不得把回生导向`BUCK_6V/SERVO_BUCK_IN`。`SERVO_V+`须由EDD-008选择独立clamp/energy path，并由PVR-015验证。

## 2. 建议冻结的正常source边界

| 项目 | Proposed requirement | 依据/影响 |
| --- | --- | --- |
| 正常外部source | XT30只连接单只可拆卸3S LiPo，正常`9.0–12.6 V` | v0.1已冻结无板载充电，外置平衡充电时默认断开机器人 |
| 禁止并接 | 运行时不得同时把充电器、第二块电池、汽车电源或未受限外部DC source接到XT30 | 否则可能形成不受本提案覆盖的持续高能load-dump |
| 实验室bench source | 只允许在受控测试中代替电池，并须由source自身限制在本文电压/能量/compliance包络内 | 不新增PCB外部供电功能 |
| 电池线束 | battery-to-PCB双线回路`L_LOOP≤2.0 µH`；总串联电阻必须按最终电芯、线规、长度、XT30和fuse holder实测 | 不允许用未知长线束覆盖设计输入；超界线束须另带外部保护或修订输入 |
| 已有断流工作点 | 最大`10 A for 10 ms`，以`10 A`、`2.0 µH`带电断开作为实际线束筛选极限 | 电感储能`0.5×L×I²=0.10 mJ`，远低于汽车load-dump，但布局振铃仍须测 |

## 3. Proposed battery-entry qualification stimulus

在电池未连接时，于PCB XT30端施加以下正向Thevenin pulse：

| Parameter | Proposed value |
| --- | --- |
| Open-circuit peak | `24 V` |
| Source resistance | `1.0 Ω` |
| Waveform | `10/1000 µs` |
| Repetitions | `10` |
| Minimum interval | `60 s` |
| Ambient | `40 °C` |
| Polarity | positive；另做`−12.6 V DC`反接测试 |

`24²/1 Ω×1 ms=0.576 J/pulse`只是把source在1 ms内可提供的能量作保守上界，真实10/1000波形和TVS工作点须用波形积分，不得把0.576 J误写成TVS实际吸收能量。

pass requirements：

- TVS焊盘处峰值`≤24.4 V`；
- 每颗`HD303N080SG`和`TPS70933`输入引脚实际探测峰值`≤27.0 V`，为30 V器件边界保留至少3 V布局/批次余量；
- TVS无裂纹、短路、明显参数漂移或不可接受温升；
- LM74800/INA300/ESTOP不得产生不可恢复损坏或违反既有MASTER-cycle恢复语义；
- pulse后重新执行battery protection、PVR-017反灌和正常上电检查。

选择`SMCJ15CA / C151273`仍只是EDD-002 preferred candidate。其目录`24.4 V/61.5 A/10×1000 µs`不能替代40 °C derating、实际工作点、pulse积分和成板寄生测量。入口保持双向TVS；单向器件在反接时形成crowbar，不进入该位置。

## 4. 实际线束与布局 PVR

最终电池/线束在9.0/12.6 V、MASTER ON/OFF、空载和`10 A/10 ms`状态下，执行插入与受控断开至少100次。使用短地弹簧/差分探头直接测：XT30、TVS两端、两颗FET各自VDS、TPS709输入、`VBAT_PROTECTED/SERVO_BUCK_IN`；不能只测远端test point。

记录每次峰值、ring frequency/decay、TVS current/energy积分、器件温度和保护状态。任一引脚超过第3节限制、出现source fighting、误熔断、不可恢复latch或器件退化即FAIL；fallback为缩短/约束线束、降低布局寄生、提高器件电压余量或重选TVS，不得删除保护功能。

## 5. Servo regeneration独立输入缺口

现有`6 A continuous / 8 A for 1 s`是正向供电包络，不能推出电机回生的反向峰值、持续时间或重复率。当前只允许用`−8 A for 20 ms`、最多`10 events/min`作为候选clamp的**非权威筛选起点**；按6.8 V粗算约`1.1 J/event`。该数值不得写入Freeze，直到最终舵机、机械减速/回弹行为和允许最高电压有证据。

需要后续完成：

- 最终舵机允许最高供电电压；
- 八路最坏反向`I-t-repeat`实测或保守推导；
- `SERVO_V+` local clamp的exact MPN、pulse derating、温升和寿命；
- 可控双向电子负载注入后，再在固定、限位、限流、人员净空和ESTOP条件齐备时做实体舵机验证。

## 6. 关闭 DIB-013 前需要 Operator 确认

1. **ACCEPTED 2026-08-31**：“XT30正常运行只连接单只3S LiPo；不并接充电器、第二电池、汽车电源或未受限DC source”；
2. 接受`L_LOOP≤2.0 µH`作为线束上限，并在最终线束上实测；
3. 接受第3节`24 V/1 Ω/10/1000 µs/10次/≥60 s/40 °C`作为入口qualification stimulus；
4. 接受battery-entry TVS不负责servo regeneration，后者继续由EDD-008/PVR-015关闭。

第2–4项确认前，本文保持Proposed，DIB-013继续OPEN；但不阻塞USB、数字、AON逻辑、SY8388被动件资料审计等独立工作。
