# Power architecture independent of EDD-002 v0.1

状态：**Topology selected / exact power-switch MPNs pending**。本文故意把电池入口有源保护视为黑盒 `VBAT_PROTECTED`，不依赖 LM74800 反灌或 TVS blocker 的最终答案。

## 1. AON control domain

```text
Battery fuse output → battery AON 3.3 V ┐
                                        ├→ CONTROL_AON_3V3
USB POWER VBUS → USB AON 3.3 V ─────────┘
```

`CONTROL_AON_3V3` 仅供：Type-C advertisement detection、MASTER/ESTOP latch、source-selection logic、fault latches和少量 status pull-ups。它不供 ESP32、CH32、显示、audio 或 servo。

两路 AON 必须使用低漏电 diode/ideal-diode OR，且任一源缺失/反接时不得向其端口反灌。AON 总预算应保持在毫安级。

## 2. Logic-source topology

```text
VBAT_PROTECTED → BAT_LOGIC_SWITCH ┐
                                  ├→ LOGIC_SOURCE → wide-input 3.40 V buck → 3V3
USB5_PROTECTED → USB_LOGIC_SWITCH ┘
```

选择硬件 USB priority 和 break-before-make，不追求无缝切换：

- USB Default advertisement：两条 main-logic switch 均 OFF；只有 AON detection 域工作；
- USB 1.5 A：USB_LOGIC_SWITCH ON，BAT_LOGIC_SWITCH OFF；只允许 minimum logic，屏幕背光、audio、3V3 expansion默认 OFF；
- USB 3 A：USB_LOGIC_SWITCH ON，BAT_LOGIC_SWITCH OFF；允许 full logic；
- USB不合格/不存在且 battery有效：BAT_LOGIC_SWITCH ON；
- MASTER OFF：两条 switch均 OFF。

源切换时允许 3V3 进入确定性 supervisor reset，而不是用数十毫法 hold-up 电容强行维持 2.5 A。原因：即使只维持 1 ms、允许 0.2 V 下跌，也需要 `C=I·dt/ΔV=12.5 mF`，不符合本板成本和面积。所有 source transfer 都先关闭 servo、disable PWM、break-before-make，再重新启动逻辑。

这满足冻结输入“保持窗口，或进入确定性受控 reset/shutdown”的选项。

## 3. 3V3 regulator requirements

选择单颗 wide-input synchronous buck：

- input recommended range必须覆盖受保护 USB最低电压至 12.6 V battery，目标至少 4.5–18 V；
- output 3.40 V nominal，完整 tolerance/load/source transient保持3.30–3.50 V；
- continuous output ≥2.5 A；3.0 A for ≥10 ms；
- hardware EN，外部下拉，PG/RESET coordination；
- 40 °C ambient下稳态 TJ≤100 °C；
- exact compensation/inductor/capacitors来自所选 manufacturer reference design；
- shutdown时不得通过 SW/FB/PG 向未供电 LOGIC_SOURCE反灌。

不需要 buck-boost，因为所有允许的 main input 均高于 3.5 V。

## 4. Servo-source topology

```text
BUCK_6V       → BUCK_SERVO_ISO ┐
                               ├→ SERVO_SOURCE → common shunt/sense → SERVO_V+
USB5_PROTECTED→ USB_SERVO_ISO ┘
```

两条路径均使用能双向关断的 back-to-back switch，且 hardware interlock保证 break-before-make。选择规则：

- battery有效时：优先 `BUCK_6V`；USB servo path保持 OFF，即使 USB 为3 A；
- 无battery且 USB为3 A：允许 USB servo path，以总输入限流为上限做降载调试；
- USB Default/1.5 A：USB servo path硬件禁止；
- ESTOP、MASTER OFF、source fault、控制域 brownout：两条 servo path均 OFF；
- source transition：先两条 OFF并等待 rail下降，再只开目标路径，不要求舵机不断电。

该优先级避免插入 USB 时把正常6 V servo rail切到5 V，也避免不必要的 source fighting。

## 5. Common servo measurement

选择 common high-side shunt + unidirectional current-sense amplifier作为前向电流遥测：

- preliminary shunt: 5 mΩ Kelvin, ≥1 W, ≤1%, low TCR；
- amplifier target gain: 50 V/V；
- 6 A→1.50 V，8 A→2.00 V，12 A→3.00 V；
- output to ESP32 GPIO5 ADC through RC/filter and overvoltage clamp；
- shunt drop: 30 mV at6 A、40 mV at8 A，计入板内压降预算。

它不承担安全过流关断；battery path OCP和USB path input-current limit分别保护各自 source。Servo regeneration/negative current不由单向 amplifier测量，使用 source isolation、output clamp和PVR测试处理。

## 6. Power-state table

| Battery | USB advertisement | MASTER | ESTOP latch | Logic source | Servo source |
| --- | --- | ---: | ---: | --- | --- |
| absent | Default | X | X | AON only | OFF |
| absent | 1.5 A | ON | clear | USB, minimum logic | OFF |
| absent | 3 A | ON | clear | USB, full logic | USB 5 V derated |
| valid | none/Default | ON | clear | battery | BUCK_6V |
| valid | 1.5 A | ON | clear | USB, minimum logic | BUCK_6V |
| valid | 3 A | ON | clear | USB, full logic | BUCK_6V |
| any | any | OFF | X | AON only | OFF |
| any | any | ON | set | allowed logic state | OFF |

Any source qualification change first forces servo OFF and PWM disabled. Logic may reset during the subsequent break-before-make transfer.

## 7. Remaining exact-selection groups

This topology closes the priority and fault-state decisions but not exact MPNs. Still required:

- USB Type-C current-advertisement controller;
- USB OVP/current-limit/reverse-blocking front end;
- battery/USB logic source switches or power mux;
- 3.40 V/3 A wide-input buck;
- two servo-source back-to-back load switches/controllers;
- forward current-sense amplifier and exact common shunt;
- 6 V rail transient clamp.
