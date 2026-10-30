# 牛来快跑 PCB Controller 设计输入冻结 v0.1

状态：**Superseded / 已由 v0.2 取代；仅作历史记录**

当前唯一权威输入：[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md`](PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.3.md)。本文件不得再用于新的 Circuit Design 结论。

日期：2026-08-30

Owner：Project maintainer

相关资料：

- [`PCB_CONTROLLER_BOM_v0.1.md`](PCB_CONTROLLER_BOM_v0.1.md)
- [`PCB_CONTROLLER_BASELINE_v0.1.md`](PCB_CONTROLLER_BASELINE_v0.1.md)
- [`HARDWARE_INTERCONNECT_SPEC_v0.1.md`](HARDWARE_INTERCONNECT_SPEC_v0.1.md)
- 原始输入资料《信息补完.md》未随当前仓库归档；此名称仅保留为历史 provenance，不作为当前设计输入。
- [`Decision 008`](../../../../docs/decisions/008-pcb-controller-v0.1.md)

## 1. 用途与冻结结论

本文汇总 Operator 已确认的 PCB Controller Rev A 设计输入，作为后续电路设计的唯一输入基线。它不是原理图、网表、完成的电路设计或样机证据。

Operator 已在 2026-08-30 接受本文及以下最终关闭方式：

1. 扬声器按尺寸、额定值和图片匹配到最可能的 XHXDZ `3516BOX-4Ω2W-2P1.25`，板端采用 `PicoBlade/MX1.25` 兼容 2P，并增加直接焊线备用孔；
2. 四角 M3 安装孔采用第 7.1 节参数；
3. 3S 电池实际质量和插件装配方仍可在不改变电气连接的前提下后补；
4. 旧 2S/5 A BOM 和 Baseline 只作历史计算参考，后续不得套用其电池阈值或舵机功率器件。

本次冻结不等于已授权发布、下单、实体测试或合并，也不证明电路可行。后续电路设计必须按本输入重新选型、计算和审查。

## 2. 已接受的系统边界

### 2.1 电池与电源入口

| 项目 | 冻结输入 | 状态 / 说明 |
| --- | --- | --- |
| 电池化学与串数 | 3S LiPo | **Accepted**；不采用原 BOM 的 2S 基线 |
| 工作电压 | 9.0-12.6 V 设计范围 | **Accepted design range**；最终低压策略仍须在电路设计中计算 |
| 容量/倍率 | 650 mAh、60C | **Accepted**；无可获得的准确 SKU |
| 主连接器 | XT30 | **Accepted** |
| 电池包络 | 61 × 30.5 × 11.5 mm | **Accepted mechanical input**；无 SKU，实际到货仍需复测 |
| 电池路径 | 电池先进入主 PCB，再由 PCB 输出给外置 UBEC | **Accepted**；取消板外 Y 线方案 |
| 板载充电 | 不包含 | **Accepted**；使用外置 3S 平衡充电器，充电时默认断开机器人 |

3S 变更要求重新计算电池 UVLO、OVLO、ADC 分压、TVS、输入电容耐压、保险和 3.3 V buck 热负荷。旧 2S 数值不得复制到新设计。

### 2.2 UBEC 与舵机供电

| 项目 | 冻结输入 | 状态 / 说明 |
| --- | --- | --- |
| UBEC | YSIDO UBEC，标称 6 A、最大 8 A | **Accepted procurement input**；输入 7-25.5 V |
| UBEC 输出 | 固定选择 6.0 V | **Accepted**；装机后禁止切到 5 V 或 7.4 V |
| 舵机母线目标 | 6 A 连续、8 A 短时 | **Accepted design target** |
| 正常舵机电源 | 外置 UBEC 6 V | **Accepted** |
| 备用舵机电源 | USB POWER 5 V，仅调试/降载 | **Accepted**；不承诺八舵机同时运动或堵转 |
| 舵机数量 | 8 × MG90S | **Accepted**；每条腿 2 个 |
| 舵机线序 | 棕=GND、红=电源、橙=PWM | **Accepted interface input** |

当前 BOM 中 5.5 A 级舵机电源 OR-ing 器件不满足新的 6 A 连续目标，后续电路设计必须重新选型或重构，不得直接沿用并宣称达标。

### 2.3 主开关与急停

| 项目 | 冻结输入 | 状态 / 说明 |
| --- | --- | --- |
| 主电源操作件 | PCB 边缘可操作的保持型拨动开关 | **Accepted** |
| 主开关电气职责 | 控制硬件 `MASTER_ENABLE`，不直接承载全部电池电流 | **Accepted** |
| 主开关关闭范围 | 禁止电池逻辑支路、UBEC 输入支路和 USB POWER 主电源支路启动 | **Accepted requirement** |
| 急停位置 | 机身外部，通过线束接主板 | **Accepted** |
| 急停触点 | 常闭干接点 | **Accepted**；断线、拔出或按下均进入急停 |
| 急停作用 | 硬件同时关闭 UBEC 和 USB 两条舵机供电路径 | **Accepted**；不依赖 MCU 或固件 |

主开关关闭后允许 USB POWER 口的毫安级 Type-C 检测域保持待机，但不得因此启动主逻辑或舵机。USB DATA 的 VBUS 始终不作为系统电源。

## 3. 通信与调试接口

| 接口 | 冻结输入 | 状态 / 说明 |
| --- | --- | --- |
| USB DATA | ESP32-S3 原生 USB Serial/JTAG | **Accepted**；电脑串口、烧录、日志和调试共用此口 |
| 专用控制 UART 插座 | 删除 | **Accepted**；释放原计划 GPIO39/40 |
| UART0 恢复 | 保留 TX/RX 焊盘 | **Accepted**；不作为日常外接控制口 |
| BOOT / RESET | 各保留一个可触达按键 | **Accepted** |
| 恢复测试点 | GPIO0、EN、UART0_TX、UART0_RX | **Accepted** |
| USB POWER | 独立 Type-C 电源口 | **Accepted**；与 DATA 口在位置和丝印上明确区分 |

## 4. 外设与首板装配范围

### 4.1 首板装配

| 功能 | PCB 预留 | 首板装配/连接 | 状态 |
| --- | ---: | ---: | --- |
| 舵机 | 8 路 | 8 路接口 | **Accepted** |
| 足端开关 | 4 路 | 4 路接口 | **Accepted** |
| 线性霍尔 | 4 个焊位 | 装 1 个，3 个 DNP | **Accepted** |
| PDM 麦克风 | 1 个 | 装 1 个 | **Accepted** |
| I2S 功放 | 1 个 | 装 1 个 | **Accepted** |
| RGB LED | 4 个 | 装 4 个 | **Accepted** |
| 红外接收 | 不设专用电路 | 外置模块占用通用 3.3 V GPIO 口 | **Accepted** |
| 红外发射 | 不预留 | 不安装 | **Accepted** |

### 4.2 冻结器件

| 功能 | 器件 | 状态 / 约束 |
| --- | --- | --- |
| MCU | `ESP32-S3-WROOM-1-N16R8` | **Accepted** |
| PWM | `PCA9685PW,118` | **Accepted**；8 路舵机输出，其他通道不驱动可见 LED |
| IMU | `ICM-42670-L` | **Accepted candidate**；设计前必须取得并核对准确 L 后缀资料 |
| 霍尔 | Hallwee `HAL49ESO`，LCSC `C7420984` | **Accepted**；设计前逐脚核对制造商资料 |
| 麦克风 | LinkMems `LMD3526B261-OFA03`，JLC `C41347997` | **Accepted**；3.3 V PDM、底部声孔 |
| 功放 | `MAX98357A` | **Accepted function**；优先改用 3 × 3 mm TQFN 后缀，避免 0.4 mm WLP |
| LED | `WS2812C-2020-V6` | **Accepted**；4 颗串联，3.3 V，固件限制亮度 |

## 5. 外接接口

| 接口 | 数量 | 电气定义 | 状态 / 说明 |
| --- | ---: | --- | --- |
| 舵机 | 8 | `GND / SERVO_V+ / PWM` | **Accepted**；2.54 mm，与 MG90S 兼容 |
| 足端开关 | 4 | `GND / SW` | **Accepted**；干接点到地，NO/NC 由接线和固件极性配置 |
| 通用传感器 | 2 | `3V3_PROTECTED / GND / GPIO / ADC_GPIO` | **Accepted**；其中一路可接外置红外接收器 |
| I2C 扩展 | 1 | `3V3_PROTECTED / GND / SDA / SCL / INT` | **Accepted**；GPIO11/12 为共享 SDA/SCL，只接短线 3.3 V 外设 |
| 通用 GPIO | 至少 2 组 | 固定针序的 3.3 V、GND 和剩余 GPIO | **Accepted**；允许杜邦线，但每个板端针脚仍必须在网表和丝印中固定 |
| 急停 | 1 | `ESTOP_LOOP_A / ESTOP_LOOP_B` | **Accepted**；外接常闭干接点 |
| UBEC 输入 | 1 | `UBEC_6V / GND` | **Accepted**；连接器及线材满足 6 A 连续目标 |
| UBEC 电池输出 | 1 | `VBAT_SWITCHED / GND` | **Accepted**；由主板供给外置 UBEC 输入 |

### 5.1 GPIO 扩展冻结

扩展接口必须引出 **15 路 ESP32-S3 GPIO**。其中 12 路为删除专用控制 UART、板载红外发射和专用红外接收后得到的无条件空闲 GPIO；GPIO7/8/9 为必须引出的 `HALL_SHARED` 共享 GPIO：

| GPIO | 分类 | 能力 / 约束 | 默认建议用途 |
| --- | --- | --- | --- |
| GPIO7 | `HALL_SHARED` | ADC1 / 数字 GPIO；通过默认 DNP 的 0 Ω 连接 Hall 2 | 扩展 GPIO；装 0 Ω 后改为 Hall 2 |
| GPIO8 | `HALL_SHARED` | ADC1 / 数字 GPIO；通过默认 DNP 的 0 Ω 连接 Hall 3 | 扩展 GPIO；装 0 Ω 后改为 Hall 3 |
| GPIO9 | `HALL_SHARED` | ADC1 / 数字 GPIO；通过默认 DNP 的 0 Ω 连接 Hall 4 | 扩展 GPIO；装 0 Ω 后改为 Hall 4 |
| GPIO1 | `FREE` | ADC1 / 数字 GPIO | 模拟或通用传感器 |
| GPIO2 | `FREE` | ADC1 / 数字 GPIO | 模拟或通用传感器 |
| GPIO10 | `FREE` | ADC1 / 数字 GPIO | 模拟扩展 |
| GPIO13 | `FREE` | 数字 GPIO | 外置红外接收器候选 |
| GPIO15 | `FREE` | 数字 GPIO | 通用扩展 |
| GPIO38 | `FREE` | 数字 GPIO | 通用扩展 |
| GPIO39 | `FREE` | 数字 GPIO | ST7789 候选 |
| GPIO40 | `FREE` | 数字 GPIO | ST7789 候选 |
| GPIO42 | `FREE` | 数字 GPIO | ST7789 候选；原红外 TX 已删除 |
| GPIO45 | `FREE_STRAP` | 启动绑带脚 | 受限扩展；外设在复位时不得强拉高 |
| GPIO47 | `FREE` | 数字 GPIO | ST7789 候选 |
| GPIO48 | `FREE` | 数字 GPIO | ST7789 候选 |

ST7789 默认可使用 GPIO39/40/42/47/48 五路信号；其 `BLK` 由 TCA9539 P12 控制，不再占用 ESP32 GPIO。该映射用于保存 ADC 能力，但屏幕仍通过通用 GPIO 排针接线。

GPIO6 固定给首板 Hall 1，不进入这 15 路扩展 GPIO。GPIO7/8/9 必须进入扩展接口，并分别通过独立 0 Ω 隔离位连接 Hall 2/3/4 焊位：

```text
GPIO7 ── 0 Ω（默认 DNP）── Hall 2
GPIO8 ── 0 Ω（默认 DNP）── Hall 3
GPIO9 ── 0 Ω（默认 DNP）── Hall 4
```

首板默认不安装 Hall 2-4 及其 0 Ω，因此 GPIO7-9 可作为三路扩展 ADC1/数字 GPIO。它们必须在连接器针序、丝印和接口表中标为 `HALL_SHARED`；安装对应 0 Ω 后，该引脚与 Hall 共网，外部不得再驱动。

最终扩展数量固定为：`12 × 无条件空闲 + 3 × HALL_SHARED = 15 × ESP32 GPIO`。15 路必须全部出现在可插接扩展接口上，不得只做测试焊盘。

除上述 ESP32 GPIO 外，允许在空间足够时引出 TCA9539 P17 作为一路低速 GPIO，并将 PCA9685 CH8-CH15 作为八路同频 PWM 测试焊盘；二者均不得标成 ESP32 通用 GPIO。

## 6. 屏幕与扬声器

### 6.1 ST7789

- 外置 170 × 320 ST7789 模块；针序由模块标为 `GND / VCC / SCL / SDA / RES / DC / CS / BLK`。
- 使用 3.3 V 供电和 3.3 V GPIO；通过杜邦线接通用 GPIO 排针，不设置专用屏幕连接器。
- 屏幕 3.3 V 必须来自受保护的外设支路，设计预算暂按最大 0.5 A。
- `SCL` 在此接口表示 SPI clock，`SDA` 表示 SPI MOSI，不接板上 I2C 总线。
- 模块最大电流和 `BLK` 内部接法未知，作为外接模块风险保留；主板不得依赖其内部保护。

### 6.2 扬声器

| 项目 | 输入 |
| --- | --- |
| 类型 | 3516 腔体扬声器 |
| 最可能准确型号 | XHXDZ `3516BOX-4Ω2W-2P1.25`，LCSC `C49247011` |
| 阻抗/额定功率 | 4 Ω / 2 W |
| 本体包络 | 35 ± 0.2 × 16 ± 0.2 × 6.5 ± 0.15 mm |
| 线束 | 200 ± 10 mm 双绞线，图示 OD 约 0.8 mm |
| 插头判定 | 最可能为 `PicoBlade/MX1.25` 兼容两针母壳；保留原插头 |
| 板端连接器 | Megastar `ZX-MX1.25-2PZZ`，LCSC `C7430458`，1.25 mm、2P、直插 |
| 电气连接 | `SPK+ / SPK-`；BTL 两线均不得接地 |

该连接器系列是基于图片外形、`2P1.25` 命名习惯和完全一致的扬声器尺寸/额定值作出的工程推断，不是从原始采购 SKU 证明的事实。`J_SPK` 冻结为上述板端型号，同时必须在旁边增加一对明确标注 `SPK+ / SPK-` 的通孔直接焊线备用孔；若实物插头不兼容，可剪线直焊而不改 PCB。红线统一接 `SPK+`，另一根线接 `SPK-`。

## 7. PCB、装配与成本边界

| 项目 | 冻结输入 | 状态 / 说明 |
| --- | --- | --- |
| 板框 | 75 × 60 mm | **Accepted target** |
| 层数 | 4 层 | **Accepted initial stack-up** |
| 铜厚 | 外层 2 oz、内层 1 oz | **Accepted target**；下单能力与阻抗参数仍需按嘉立创选项复核 |
| SMT | 单面贴片 | **Accepted** |
| 插件器件 | 使用通孔封装；嘉立创插件或到板后手焊在报价阶段决定 | **Accepted**；不改变电气网表 |
| 制造/贴片 | 嘉立创 | **Accepted** |
| 数量 | 2 块 PCBA | **Accepted** |
| 成本目标 | 每块 100-120 元 | **Accepted target**；是否含运费、钢网和扩展库费尚不作为电气输入 |
| 工艺原则 | 非必要不使用增加加工难度或成本的封装 | **Accepted** |

### 7.1 安装孔冻结

- 4 × M3，采用 `Ø3.4 mm` 非金属化通孔；
- 四孔中心距相邻板边各 4.5 mm，名义孔距为 66 × 51 mm；
- 每孔上下两面保留至少 `Ø7.5 mm` 器件禁布区；
- 所有铜层在孔周围保留至少 `Ø5.4 mm` 铜箔禁布区；
- 安装孔不连接电气地；优先使用尼龙柱，使用金属紧固件时加绝缘垫片；
- ESP32 天线净空优先于角孔位置；发生冲突时移动对应安装孔，不得侵入天线 keep-out。

安装孔参数为 **Accepted mechanical input**。

## 8. 电源安全要求

后续电路设计至少必须包含并验证：

- 电池入口保险或等效不可绕过的故障电流限制；
- 电池反接、过压、欠压、浪涌和短路保护；
- 逻辑支路与 UBEC 输入支路的明确分支保护；
- USB POWER 只有在合格电源条件下才能进入主电源路径；
- USB DATA VBUS 仅检测、不供电且不反灌电脑；
- USB 5 V 与 UBEC 6 V 不得硬并联；
- 舵机母线满足 6 A 连续、8 A 短时目标，带电流测量、负向瞬态钳位和反向电流阻断；
- 关键功率器件具有过温保护，并在功率区加入可读取的温度监测；
- MCU 复位、掉电或未武装时 PWM 和舵机电源默认关闭；
- 外部常闭急停硬件直接关闭全部舵机电源来源；
- BAT、UBEC、SERVO_V+、USB、3V3和GND提供可安全测量的测试点；
- 首次上电和执行器测试遵守限流、固定、限位、人员净空和可触达急停要求。

任何改变舵机电压、电流、急停、保护阈值或运动安全边界的变更均按 `HIGH RISK` 处理。

## 9. 相对现有 BOM/基线的明确变化

下列冻结内容取代现有 Proposed BOM/基线中的对应假设：

1. 2S 电池改为 3S 650 mAh、60C；
2. 取消电池板外 Y 线，改为电池先进主板，再由主板输出给 UBEC；
3. 舵机母线从约 5 A 试验上限改为 6 A 连续、8 A 短时设计目标；
4. 增加板载 `MASTER_ENABLE` 拨动开关和外接常闭硬件急停；
5. 删除专用控制 UART 插座；保留原生 USB Serial/JTAG 和 UART0 恢复焊盘；
6. 删除板载红外发射与专用红外接收电路，外置接收器使用通用 GPIO；
7. 霍尔只装 1 个，其他 3 个焊位 DNP；
8. MAX98357A 优先采用 TQFN，不采用 WLP；
9. PCB 目标改为 75 × 60 mm、4 层、外 2 oz/内 1 oz、单面 SMT；
10. 增加并接受四角 M3 安装孔参数；
11. 扬声器冻结为最可能的 XHXDZ `3516BOX-4Ω2W-2P1.25`，板端采用 `ZX-MX1.25-2PZZ` 并保留直焊备用孔。

旧 BOM、Baseline 和 Decision 008 已增加指向本文的状态说明；它们不得被误认为已适配 3S/6 A 新要求。

## 10. 非阻塞后续确认

| 项目 | Owner | 关闭条件 | 对电路设计的影响 |
| --- | --- | --- | --- |
| 扬声器插头实物试配 | Operator | 到货后与 `ZX-MX1.25-2PZZ` 试配 | 不阻塞；不兼容时使用冻结的直焊备用孔 |
| 3S 电池实物质量 | Operator | 到货称重 | 不影响电气连接；影响机械/整机质量 |
| THT 由嘉立创还是手焊 | Operator | 报价阶段选择 | 不改变网表和封装 |

## 11. Operator 审阅结论

Project maintainer 于 2026-08-30 明确要求进行最终冻结，接受主板固定方式，并授权以最可能的 1.25 mm 连接器系列关闭扬声器输入。本文因此成为 Rev A 电路设计输入基线。

电路设计、原理图、网表、ERC、PCB布局、下单和实体测试均尚未开始。
