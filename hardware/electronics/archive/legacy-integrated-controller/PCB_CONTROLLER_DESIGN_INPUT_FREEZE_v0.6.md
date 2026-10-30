# 牛来快跑 PCB Controller 设计输入冻结 v0.6

状态：**Superseded by [`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.7.md)；仅作历史记录**

日期：2026-08-30

Owner：Project maintainer

被取代版本：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.5.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.5.md)

## 1. 权威性与合并规则

本文曾是 PCB Controller Rev A Circuit Design 权威电气设计输入，并完整取代 v0.5；现已由v0.7取代。本文只记录删除外置UBEC接口和固定板载SY8388唯一6 V source的历史修订，不得覆盖v0.7。

本修订来自 Project maintainer 于 2026-08-30 的明确决定：删除 v0.5 保留的外置 UBEC fallback 和两套 UBEC 2P connector。`Silergy SY8388ARHC / JLC C5110279` 板载同步降压成为 Rev A 唯一正常 6.0 V 舵机电源实现。旧冻结版本、BOM、Baseline、Decision、商城资料和聊天不得覆盖本文。

## 2. 删除外置 UBEC 接口

删除以下 v0.2/v0.5 requirement：

- 外置 UBEC 电池输入 2P connector；
- 外置 UBEC 6 V 输出 2P connector；
- YSIDO 或其他外置 UBEC fallback；
- `ONBOARD_BUCK` / `EXTERNAL_UBEC_FALLBACK` 互斥装配选项；
- 为外置 UBEC 保留的 DNP connector、selector、jumper和assembly configuration。

不得把原已选 `WJ2EDGVC-5.08-02P-14-00A / C8445`、`WJ2EDGK-5.08-02P-14-00A / C71370` 或其他 UBEC connector 带入当前 Circuit Design net/component/assembly table。它们只保留为历史选型记录。

不得通过普通排针、用户可接触端子或未受保护的注入路径重新引入外置 6 V source。若未来需要恢复外置 UBEC、外部 6 V输入或 fallback source，必须再次修订权威 Design Input Freeze并重新完成反灌、ESTOP、保护和source-selection设计。

## 3. 唯一 6 V source

正常 6 V power tree 固定为：

```text
3S LiPo
  → physical fuse
  → active battery protection
  → SERVO_BUCK_IN
  → Silergy SY8388ARHC / C5110279
  → BUCK_6V
  → servo-source isolation / protection / current sense
  → SERVO_V+
```

v0.5 §2 对 `SY8388ARHC`、6.0 V nominal、6 A continuous、8 A/1 s/≥10 s、输入支路包络、hardware EN/ESTOP、default-safe、计算和prototype validation的全部要求保持不变。

Circuit Design 仍须在 `SERVO_BUCK_IN`、`BUCK_6V` 和 `SERVO_V+` 提供适合电压/纹波/动态/故障测量的 test points。此类 test points 是实验室测量节点，不是外部电源接口，不构成 fallback source，也不得允许正常使用中绕过保护或 ESTOP。

## 4. 不变要求

除第 2、3 节明确删除的 UBEC fallback/interface 外，v0.5 以及其规范性纳入的更早条款全部保持不变，包括：

- `SY8388ARHC / C5110279` 板载 6.0 V buck；
- 3S LiPo、battery-entry physical fuse、active battery protection及故障覆盖；
- USB 5 V 调试/降载舵机 source 与板载 6 V source 的隔离/选择；
- MASTER_ENABLE、外部常闭 ESTOP、硬件锁存和 MASTER OFF→ON 恢复；
- `CH32V003F4P6 + 74HC244PW,118` 八路 hardware-timed PWM architecture；
- Circuit Design Freeze、review和阶段门禁。

## 5. 阶段与证据边界

本修订只改变权威电气设计输入，不完成也不授权新的计算，不授权 schematic、ERC、netlist、PCB layout、采购、下单或实体测试。

下一步仍属于 Circuit Design：完成板载 buck及全部其余电路的可复现计算、器件选择、pin-by-pin connection、power/fault/default-state tables、layout constraints和prototype validation plan。只有 Proposed Circuit Design Freeze 经规定的人类 review和Operator明确接受后，才允许进入原理图阶段。
