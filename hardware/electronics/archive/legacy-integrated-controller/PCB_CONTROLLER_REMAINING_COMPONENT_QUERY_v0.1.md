# PCB Controller Rev A — remaining component query v0.1

状态：**Operator/JLC selection input request**。本表只列不依赖 EDD-002 当前外部问询、且仍需要 exact MPN 的器件。候选不等于已选 BOM。

## A. Highest priority

### A1 — 3V3 wide-input synchronous buck

按你的约束，我会把候选分成“主选”和“低成本备选”。目前最干净的答案是：**TPS54326RGTR 最符合你的完整要求；SY8113BADC 是明显更便宜的实用备选；RT6217 系列在 24 V 裕量和瞬态上很好，但当前嘉立创库存不理想。**

| 候选                                  |     VIN 推荐范围 |               IOUT | EN | PG    | 封装                 | 嘉立创/LCSC 情况                    | 结论           |
| ----------------------------------- | -----------: | -----------------: | -- | ----- | ------------------ | ------------------------------ | ------------ |
| **TI TPS54326RGTR / C140334**       | **4.5–18 V** | **3 A continuous** | ✓  | **✓** | **VQFN-16-EP 3×3** | 约 288 pcs；约 **US$0.59 ≈ ¥4.2** | **首选**       |
| **Silergy SY8113BADC / C78989**     | **4.5–18 V** |            **3 A** | ✓  | —     | TSOT-23-6          | 约 **10k+ pcs；¥1.42/pcs**       | **成本首选**     |
| **Richtek RT6217BHGJ8F / C2976595** | **4.5–24 V** |            **3 A** | ✓  | **✓** | TSOT-23-8 (FC)     | 当前缺货；参考约 US$0.47               | 技术性能很好，库存差   |
| **Richtek RT6217EHGJ8F / C2875474** | **4.5–24 V** |            **3 A** | ✓  | —     | TSOT-23-8 (FC)     | 少量库存；约 US$0.49                 | 可作为无 PG 版本替代 |

### 1. TPS54326RGTR — 我最推荐

这是四个里面最贴近你原始 specification 的。

它是 **4.5–18 V recommended VCC、3 A continuous、同步整流、可调输出、hardware enable、PGOOD、adjustable soft-start**，而且正好是你偏好的 **3×3 mm exposed-pad VQFN**。嘉立创对应 **C140334**，当前检索到约 288 pcs 库存，1 pcs 约 US$0.59，实际人民币就是约 ¥4.x，满足你的 ¥6 目标。([LCSC Electronics][1])

TI 明确给出：

* VIN/VCC：4.5–18 V
* VOUT：0.76–5.5 V
* IOUT：3 A
* synchronous rectification
* EN
* PGOOD
* cycle-by-cycle OCP
* adjustable soft start
* QFN exposed-pad 封装。([Texas Instruments][2])

3.40 V 很容易设定。VREF = 0.76 V，

$$
V_{OUT}=0.76(1+R_{TOP}/R_{BOT})
$$

若取：

* RBOT = 10.0 kΩ
* RTOP = **34.8 kΩ**

则：

$$
V_{OUT}\approx0.76(1+34.8/10)=3.4048V
$$

非常合适。

原厂 datasheet：

[TI TPS54326 原厂产品页 / Datasheet](https://www.ti.com/product/TPS54326?utm_source=chatgpt.com)

嘉立创：

[TPS54326RGTR / C140334](https://item.szlcsc.com/151660.html?utm_source=chatgpt.com)

datasheet 中包含完整的 output-divider、inductor、input/output capacitor、soft-start 与 layout 指导，并且 TI 还有对应 3 A EVM。([Texas Instruments][2])

**如果这个 3V3 rail 是 ESP32 + 数字逻辑 + 传感器等核心系统电源，我现在会直接选它。**

---

### 2. SY8113BADC — 最便宜，但没有 PG

如果 PG 不是硬要求，这个非常有竞争力。

嘉立创 **C78989**，目前库存非常充足，商城价格约 **¥1.42/pcs @5pcs**。规格是：

* VIN = **4.5–18 V**
* synchronous buck
* **3 A**
* 500 kHz
* adjustable
* hardware EN
* cycle-by-cycle current limiting
* internal soft-start
* thermal shutdown
* TSOT-23-6。([立创商城][3])

内部上下 MOSFET 典型 RDS(on) 约 **80 mΩ / 40 mΩ**，而且 datasheet 给出的 top-FET peak limit 为约 6 A、bottom valley current limit typ. 4.25 A，因此从电流保护裕量看，**2.5 A continuous + 3 A/10 ms 并不是勉强工作状态**。([Scribd][4])

3.40 V 设置也很简单，VREF = 0.600 V：

$$
V_{OUT}=0.6(1+R_{TOP}/R_{BOT})
$$

例如：

* RBOT = 10.0 kΩ
* RTOP = **46.4 kΩ**

得到约：

$$
3.384V
$$

若希望更接近 3.400 V，可以组合成约 46.7 kΩ，得到约 3.402 V。

它的 application note 确实包含：

* CIN recommendation；
* inductor selection；
* output capacitance；
* feedback；
* complete typical circuit；
* layout considerations。([Scribd][4])

但有两个缺点：

**第一，没有 PGOOD。**

第二，是 **TSOT-23-6，而不是你更偏好的 QFN/SO8 exposed pad**。

另外我目前能检索到的是 Silergy 原厂文件的授权/分销镜像，而不是一个稳定可访问的 Silergy 官方公开 URL，所以如果你坚持“链接本身必须 silergy.com 域名”，我不会把这一项伪装成满足。

嘉立创：

[SY8113BADC / C78989](https://item.szlcsc.com/m/80126.html?utm_source=chatgpt.com)

所以它更适合：

> **成本敏感的 3V3 rail，且系统不依赖 PG 做 power sequencing。**

---

### 3. RT6217BHGJ8F — 规格很好，但现在不建议冻结进 BOM

这个实际上在电气性能上比 TPS54326 更宽松：

* **4.5–24 V recommended**
* 3 A continuous
* synchronous
* 85 mΩ / 40 mΩ MOSFET
* EN
* **PGOOD**
* externally adjustable soft-start
* UVLO / UVP / OTP
* 500 kHz
* VOUT 0.791–6 V。([richtek.com][5])

尤其 24 V VIN rating 对你的“wide input”很舒服，即使名义 12 V rail 有一点输入尖峰，也比 18 V 器件安心。

VREF = 0.791 V，3.40 V 可设成：

* RBOT = 10.0 kΩ
* RTOP ≈ **33.0 kΩ**

得到：

$$
0.791(1+33/10)\approx3.401V
$$

非常漂亮。

原厂 datasheet：

[Richtek RT6217A/B 原厂 Datasheet PDF](https://www.richtek.com/assets/product_file/RT6217A%3DRT6217B/DS6217AB-01.pdf?utm_source=chatgpt.com)

原厂产品页：

[Richtek RT6217A/B](https://www.richtek.com/Products/Switching%20Regulators/DC_DC%20StepDown%20Convertor/RT6217ART6217B?sc_lang=en&utm_source=chatgpt.com)

但嘉立创 C2976595 当前检索是**缺货**，参考价格约 US$0.47。([LCSC Electronics][6])

所以我不会拿它作为现在 PCB freeze 的首选。

---

### 4. RT6217EHGJ8F — 有一点现货，但没有 PG

它基本是 RT6217 家族的简化版：

* 4.5–24 V
* 3 A
* synchronous
* EN
* PSM / forced PWM selectable
* UVLO / UVP / thermal shutdown
* 85/40 mΩ MOSFET
* 500 kHz。([LCSC Electronics][7])

但 **RT6217E 没有 PGOOD**。而且当前嘉立创只有个位数到十几颗量级库存，适合样机，不适合我现在推荐你做长期 BOM。

原厂：

[Richtek RT6217E 产品页及 Datasheet](https://www.richtek.com/Products/Switching%20Regulators/DC_DC%20StepDown%20Convertor/RT6217E?sc_lang=zh-CN&utm_source=chatgpt.com)

---

## 我的筛选结果

如果把你的要求按优先级真正落实，而不是单纯找“能降到 3.3 V 的 3 A 芯片”，我会收敛成：

**A. 首选：TPS54326RGTR / C140334**

原因是它几乎一次满足全部条件：

> 4.5–18 V → 3.40 V / 3 A
> synchronous
> EN + PG
> exposed-pad QFN
> TI 完整设计资料
> 当前有货
> ≈ ¥4.x

这是目前**最没有明显短板**的方案。

**B. 降本版：SY8113BADC / C78989**

> ¥1.4 左右，库存巨大，4.5–18 V / 3 A，同步，EN。

代价就是：

> **没有 PG + TSOT23-6 + Silergy 公共资料链不如 TI 干净。**

我反而**不建议 MP2315GJ**：虽然 4.5–24 V、3 A、同步、EN 都符合，而且资料完善，但 MPS 已把它标为 **NRFND / Not Recommended for New Designs**，新板没必要现在再导入。([monolithicpower.com][8])

另外 **BD9D300MUV** 虽然 QFN、PG、EN、3 A、价格也漂亮，但 recommended VIN 只有 **4–17 V**，因此我已经把它排除，不算满足你“至少 18 V”的候选。([rohm.com][9])

**因此如果现在要冻结 exact MPN，我会选 `TPS54326RGTR (C140334)`。** 对 3.40 V 输出可直接从 **RTOP≈34.8 kΩ / RBOT=10 kΩ** 开始设计，然后再按你实际的 VIN nominal/max 和 2.5 A load profile 把 L、CIN、COUT 和热设计完整算出来。

[1]: https://www.lcsc.com/product-detail/C140334.html?utm_source=chatgpt.com "TPS54326RGTR | Voltage Regulators - DC DC Switching Regulators | TI | LCSC"
[2]: https://www.ti.com/product/TPS54326?utm_source=chatgpt.com "TPS54326 data sheet, product information and support | TI.com"
[3]: https://item.szlcsc.com/m/80126.html?utm_source=chatgpt.com "SY8113BADC电子元器件_最新报价_数据手册下载_silergy(矽力杰)-立创商城"
[4]: https://www.scribd.com/document/690983068/SY8113B-Silergy?utm_source=chatgpt.com "SY8113B Step-Down Regulator Datasheet | PDF"
[5]: https://www.richtek.com/assets/product_file/RT6217A%3DRT6217B/DS6217AB-00.pdf?utm_source=chatgpt.com "24V, 3A, 500kHz, ACOTTM Step-Down Converter in 8 Pin TSOT-23"
[6]: https://www.lcsc.com/product-detail/DC-DC-Converters_Richtek-Tech-RT6217BHGJ8F_C2976595.html?utm_source=chatgpt.com "RT6217BHGJ8F | RICHTEK | Price | In Stock | LCSC Electronics"
[7]: https://www.lcsc.com/es/product-detail/C2875474.html?is_substitute=1&lcsc_vid=FQAKVwECEQQKXlUHQgVXVFdXR1IIBABUFgMKBgIDElkxVlNeQVVeVFNQTldaUTtW&original_product_code=C2976594&original_unit_price=0.2882&utm_source=chatgpt.com "RT6217EHGJ8F | Voltage Regulators - DC DC Switching Regulators | RICHTEK | LCSC"
[8]: https://www.monolithicpower.com/en/mp2315.html?utm_source=chatgpt.com "MP2315 | Power Management >> Switching Converters & Controllers >> Step-down(Buck) >> Converters"
[9]: https://www.rohm.com/products/power-management/switching-regulators/buck-step-down/integrated-fet-synchronous/bd9d300muv-product?utm_source=chatgpt.com "BD9D300MUV-E2 - Data Sheet, Product Detail | ROHM.com"


### A2 — USB POWER Type-C current detection

我筛下来，**最适合 A2 的其实不是 TUSB320，而是 onsemi FUSB303/FUSB303B**。原因是它在 GPIO 模式下直接给出你要的 Default / 1.5 A / 3 A 硬件状态，而且还能区分“未连接”；完全不需要 MCU 才能做舵机电源许可。TUSB320 也满足基本要求，但有一个值得注意的限制：GPIO 只保证反映**attach 时的初始电流 advertisement**，如果 source 后续改变 Rp 档位，TI 明确建议改用 I²C + soft reset 才能更新检测结果。([Texas Instruments][1])

| 优先级       | Exact MPN          | 嘉立创/LCSC C-code | 封装                    | GPIO 硬件区分 3 档 | 供电         | 当前价格参考                                       |
| --------- | ------------------ | --------------: | --------------------- | ------------- | ---------- | -------------------------------------------- |
| **1 推荐**  | **FUSB303TMX**     |     **C603072** | X2-QFN-12, 1.6×1.6 mm | **是**         | 2.85–5.5 V | **$0.5394/1pcs；$0.4296/10pcs**               |
| **1A 新版** | **FUSB303BTMX**    |     **C895444** | X2-QFN-12, 1.6×1.6 mm | **是**         | 2.85–5.5 V | LCSC约 **$0.5394/pcs**；JLC-SMT页面曾显示预订参考价 ¥1.7 |
| 2         | **TUSB320LAIRWBR** |     **C132554** | X2-QFN-12, 1.6×1.6 mm | 是             | 2.7–5.0 V  | 明显贵于 FUSB303                                 |
| 3         | TUSB320LIRWBR      |               — | X2-QFN-12             | 是             | 2.7–5.0 V  | 不建议特意选                                       |

FUSB303TMX 当前 LCSC 页面给出 5069 颗库存，1+ $0.5394，10+ $0.4296，30+ $0.3738；FUSB303BTMX 也是同等级价格。([LCSC Electronics][2])
[FUSB303TMX 嘉立创/LCSC 商品页](https://www.lcsc.com/product-detail/USB-ICs_onsemi-FUSB303TMX_C603072.html?utm_source=chatgpt.com)
[FUSB303BTMX 嘉立创 SMT 商品页（C895444）](https://www.jlc-smt.com/lcsc/detail/C895444.html?utm_source=chatgpt.com)
[FUSB303B 原厂 datasheet](https://www.onsemi.com/pdf/datasheet/fusb303b-d.pdf?utm_source=chatgpt.com)
[FUSB303 原厂 datasheet](https://www.onsemi.com/download/data-sheet/pdf/fusb303-d.pdf?utm_source=chatgpt.com)

### 1. 首选：FUSB303TMX / C603072

这是目前我认为**成本最低且功能完整满足**的方案。

原厂明确规定：

* `CC1`, `CC2`：直接接 Type-C receptacle 的 CC1 / CC2；
* `PORT/DEBUG_N`：上电/EN 时采样，**LOW = Sink Only**；
* `ADDR/ORIENT`：**Floating = GPIO mode**；
* `OUT1`, `OUT2`：open-drain GPIO；
* `VDD = 2.85–5.5 V`；
* 具有 **Dead Battery Sink**，即即使芯片 VDD 尚未建立，也能在 CC 上呈现 Rd；
* `VBUS_DET` 直接用于 VBUS attach/detach 检测，CC/VBUS_DET 均耐 28 V。([onsemi][3])

所以你的 UFP-only 连接可以直接做成：

```text
USB-C receptacle
CC1 -------------------- FUSB303 CC1
CC2 -------------------- FUSB303 CC2
VBUS ------------------- FUSB303 VDD
   \-------------------- FUSB303 VBUS_DET

PORT/DEBUG_N ---- ~900 kΩ ---- GND     -> Sink only
ADDR/ORIENT ---------------- NC        -> GPIO mode
EN_N ------------------------ GND      -> always enabled

OUT1 ---- pull-up -> USB_AON / logic rail
OUT2 ---- pull-up -> USB_AON / logic rail
```

原厂建议 `PORT/DEBUG_N` 接 VDD/GND 时使用约 **900 kΩ**，以降低待机电流；`ADDR/ORIENT` 浮空即选择 GPIO。([onsemi][3])

最重要的是它的硬件 truth table：

| 状态                  |    OUT1 |    OUT2 |
| ------------------- | ------: | ------: |
| **未连接**             |    Hi-Z | **LOW** |
| **Default current** |    Hi-Z |    Hi-Z |
| **1.5 A**           | **LOW** |    Hi-Z |
| **3.0 A**           | **LOW** | **LOW** |

这是 onsemi 原厂 datasheet 的 pin table，不需要 MCU 或 I²C。([onsemi][3])

因此用上拉后，把 Hi-Z 视为逻辑 1：

| 状态        | OUT1 logic | OUT2 logic |
| --------- | ---------: | ---------: |
| no attach |          1 |          0 |
| Default   |          1 |          1 |
| 1.5 A     |          0 |          1 |
| **3 A**   |      **0** |      **0** |

这对你的安全链特别好，因为：

```text
SERVO_POWER_ALLOW = !OUT1 & !OUT2
```

就可以用一个 **NOR / 等效门逻辑**得到“仅 3 A advertisement 才允许舵机供电”。

换句话说：

```text
Default  -> servo OFF
1.5 A    -> servo OFF
3 A      -> servo ON
detached -> servo OFF
```

而且“未连接”和 3 A 不会混淆。

FUSB303 还定义了 sink 端 CC 电压阈值，分别对应 Default、1.5 A、3 A，因此这不是依靠模糊模拟比较器自己猜档位，而是完整的 Type-C CC state machine。FUSB303B 的典型边界为 Default / 1.5A / 3A 检测约 `0.61–0.70 V`、`1.16–1.31 V`、`2.04–2.18 V` 这些规定区域。([onsemi][3])

### 2. FUSB303BTMX / C895444

如果 BOM 系统里新版 B 比 FUSB303 更容易采购，我会接受它作为 **等价首选**。

Exact information：

**MPN:** `FUSB303BTMX`
**C-code:** `C895444`
**Package:** `X2-QFN-12 (1.6 × 1.6 mm)`
**VDD:** `2.85–5.5 V`

嘉立创 SMT 页面已经明确对应 `FUSB303BTMX / C895444`。([JLC SMT][4])

它的 GPIO truth table 与上面相同，datasheet 原文明确为：

```text
No device:     OUT1=Hi-Z, OUT2=LOW
Default sink:  OUT1=Hi-Z, OUT2=Hi-Z
1.5 A sink:    OUT1=LOW,  OUT2=Hi-Z
3 A sink:      OUT1=LOW,  OUT2=LOW
```

([onsemi][3])

而且 B 版还有一个对安全设计很有价值的特点：内部 `BC_LVL` 会检测 advertisement level 的变化，并能产生 change event。这说明内部 CC level detection 并不是只在 attach 时采样一次。([onsemi][3])

所以在 **FUSB303 与 FUSB303B 同价时，我略偏向 FUSB303B**。

---

### 3. TUSB320LAIRWBR / C132554

这是你给的搜索起点中可以用的 exact suffix。

**MPN:** `TUSB320LAIRWBR`
**C-code:** **C132554**
**Package:** `RWB / X2QFN-12 / 1.6 × 1.6 mm`
**VDD:** `2.7–5.0 V`。([Texas Instruments][5])

[嘉立创 TUSB320LAIRWBR C132554](https://item.szlcsc.com/143853.html?utm_source=chatgpt.com)
[TI TUSB320LAI 产品页](https://www.ti.com/product/TUSB320LAI?utm_source=chatgpt.com)
[TI TUSB320LAI 原厂 datasheet](https://www.ti.com/lit/ds/symlink/tusb320lai.pdf?utm_source=chatgpt.com)

UFP-only GPIO 接法同样非常直接：

```text
CC1  -> USB-C CC1
CC2  -> USB-C CC2

PORT -> GND               UFP only
ADDR -> NC                GPIO mode

VBUS -> 900 kΩ -> VBUS_DET
VDD  -> 5 V VBUS / AON
EN_N -> LOW
```

TI 明确规定：

* `PORT = LOW` → UFP only；
* `ADDR = NC` → GPIO mode；
* VBUS 到 VBUS_DET 必须有约 **900 kΩ**；
* OUT1/OUT2 是 open-drain。([Texas Instruments][1])

它的 truth table 是：

| OUT1 | OUT2 | 状态                   |
| ---: | ---: | -------------------- |
|    H |    H | Default / unattached |
|    H |    L | Default / attached   |
|    L |    H | 1.5 A                |
|    L |    L | 3.0 A                |

([Texas Instruments][1])

因此同样可以：

```text
3A_ONLY = NOR(OUT1, OUT2)
```

得到硬件 3 A permit。

但我没有把它排第一，除了价格，更主要是 TI 明确写道：

> GPIO OUT1/OUT2 只报告 initial advertised current。

如果已经 attach 后 DFP 将 advertisement 从 3 A 改成 Default，GPIO 不保证跟着更新；TI 推荐软件周期性 I²C soft-reset 来更新 current detect。([Texas Instruments][1])

这和你的要求：

> “不依赖 MCU 才能禁止 Default/1.5 A servo”

存在一个**安全语义上的弱点**。

它在**attach 时**完全满足，但如果你希望“只要 source 任何时刻不再保证 3 A，servo 必须硬件掉电”，我不愿意把 TUSB320 作为最佳方案。

---

## 最终建议

我会冻结 A2 为：

**A2 = onsemi FUSB303BTMX / C895444，备选 FUSB303TMX / C603072。**

硬件配置：

```text
VDD          = USB_AON_5V 或 VBUS
CC1          = Type-C CC1
CC2          = Type-C CC2
VBUS_DET     = VBUS
PORT/DEBUG_N = 900k -> GND
ADDR/ORIENT  = NC
EN_N         = GND

OUT1/OUT2    = pull-up to USB_AON logic rail
```

然后安全链：

```text
USB_3A_OK = NOR(OUT1, OUT2)

USB_3A_OK = 1 only when:
attached && Source advertises 3 A
```

再把它直接纳入 servo power enable，例如：

```text
SERVO_PWR_EN =
    USB_3A_OK
  & SYSTEM_SAFE
  & MCU_INIT_OK
```

其中 **USB_3A_OK 不经过 MCU**。

这比用 MCU 读 Type-C controller 更符合你的 failsafe 设计目标，而且芯片本身大约就是 **¥2–4 级**，明显低于 TI TUSB320。FUSB303TMX 当前全球 LCSC 1pcs 约 $0.54，并且有数千颗库存。([LCSC Electronics][2])

有一点我会在原理图冻结时特别注明：**Type-C 的 “Default current” 并不固定等于 500 mA**，它取决于 USB 模式/上游能力，因此设计判断应该始终写成 `TYPEC_3A_ADVERTISEMENT_OK`，不要把它命名成简单的 `USB_CURRENT_GT_1P5A`。这样系统安全含义会更准确。([Texas Instruments][6])

[1]: https://www.ti.com/lit/ds/symlink/tusb320.pdf?ts=1781208148043 "TUSB320 USB Type-C Configuration Channel Logic and Port Control datasheet (Rev. F)"
[2]: https://www.lcsc.com/product-detail/USB-ICs_onsemi-FUSB303TMX_C603072.html?utm_source=chatgpt.com "FUSB303TMX | Controllers | onsemi | LCSC"
[3]: https://www.onsemi.com/pdf/datasheet/fusb303b-d.pdf "FUSB303B - FUSB303B Autonomous USB Type-C Port Controller with I2C and GPIO Control"
[4]: https://www.jlc-smt.com/lcsc/detail/C895444.html?utm_source=chatgpt.com "FUSB303BTMX_onsemi(安森美)_FUSB303BTMX中文资料_PDF手册_价格"
[5]: https://www.ti.com/product/TUSB320LAI/part-details/TUSB320LAIRWBR?utm_source=chatgpt.com "TUSB320LAI | Buy TI Parts | TI.com"
[6]: https://www.ti.com/lit/ds/symlink/tusb320.pdf?ts=1781208148043&utm_source=chatgpt.com "TUSB320 USB Type-C Configuration Channel Logic and Port Control datasheet (Rev. F)"

### A3 — USB 5 V front-end switch/eFuse

可以。按你给出的 A3 硬约束重新筛过以后，我认为目前**最值得冻结的器件不是 TPS25940，而是更便宜的 `TPS25942ARVCR`**。它在嘉立创/LCSC 当前有大量库存，而且单颗价格已经降到约 **US$0.88**；功能上又恰好包含你需要的 **OVP + short protection + true reverse-current blocking + FLT + 5 A 级内部 FET**。([LCSC Electronics][1])

## A3 推荐冻结：TPS25942ARVCR / C181295

| 项目                       | `TPS25942ARVCR`                                         |
| ------------------------ | ------------------------------------------------------- |
| 厂商                       | Texas Instruments                                       |
| Exact MPN                | **TPS25942ARVCR**                                       |
| 嘉立创/LCSC                 | **C181295**                                             |
| 封装                       | **QFN/WQFN-20 EP，3 × 4 mm**                             |
| VIN operating            | **2.7–18 V**                                            |
| Absolute max             | **20 V**                                                |
| Continuous capability    | **5.2 A class**                                         |
| RON                      | **42 mΩ typ**                                           |
| Current limit            | **0.6–5.3 A programmable**                              |
| OVP                      | **有，可外部分压编程**                                           |
| Short protection         | **有，fast-trip + current limiting**                      |
| Reverse-current blocking | **有，内部 back-to-back FET，true reverse blocking**         |
| EN                       | EN/UVLO，active-high，可外部下拉实现 **default-off**             |
| Fault                    | **FLT，active-low open-drain**                           |
| PGOOD                    | 有                                                       |
| Thermal protection       | 有                                                       |
| 嘉立创当前库存                  | 约 **3315 pcs**                                          |
| 当前价格                     | **1+: $0.8785；10+: $0.7773；30+: $0.6809；100+: $0.6091** |

嘉立创页面明确列出 2.7–18 V、42 mΩ、0.6–5.3 A programmable current limit、reverse current blocking、FLT/PGOOD；当前页面显示 3315 颗库存和上述价格。([LCSC Electronics][1])

[嘉立创/LCSC — TPS25942ARVCR C181295](https://www.lcsc.com/product-detail/C181295.html?utm_source=chatgpt.com)
[TI 原厂 Datasheet — TPS25942A/TPS25942L/TPS25944A/TPS25944L](https://www.ti.com/lit/ds/symlink/tps25942a.pdf?utm_source=chatgpt.com)

### 1. VIN / OVP：完全满足

这是相当宽裕的一项：

* recommended operating：**2.7–18 V**
* 器件最大值：**20 V**
* OVP threshold 外部可编程；
* OVP 触发后内部 FET 断开，并 assert `FLT`。

所以你的要求：

> VIN ≥ 6 V，abs ≥ 7 V

实际上留出了很大的裕量。TI 还给出了 ±2% 的 OV/UV threshold 精度。([Texas Instruments][2])

对于 5 V USB front end，我建议后续把 **OVLO 设在约 5.8–6.0 V**，而不是让 USB VBUS 暴露到器件的 18 V 上限。

---

## 2. 最重要的一项：它是真正的 reverse-current blocking

这是我选择 TPS25942 而不选择普通便宜 USB load switch 的主要原因。

TPS25942 内部使用 **back-to-back FET**，并持续监测：

$$
V_{IN}-V_{OUT}
$$

当：

$$
V_{IN}<V_{OUT}-约10mV
$$

时会关闭反向路径。TI 明确把这个功能称为：

> **True Reverse Blocking**

reverse comparator 的典型 falling threshold 是 **−9.3 mV**，reverse shutoff 时间约 **1 µs**。([Texas Instruments][2])

这正好解决你 A3 中非常关键的一条：

> output short不得由battery/other source持续反灌。

例如：

```text
USB-C VBUS
    │
    ▼
 TPS25942
 IN     OUT ───────── SYS_5V / downstream rail
          │
          └──── other source / battery-derived supply
```

如果 downstream rail 被另一路电源抬高，而 USB VBUS 消失：

```text
USB VBUS = 0 V
SYS_5V   = 5 V
```

TPS25942 会切断：

```text
SYS_5V ──X──> USB VBUS
```

而不是像单 NMOS load switch 那样留下 body-diode 回灌路径。TI 给出的关断状态测试甚至包括 `VIN=2.7 V, VOUT=18 V` 的反向 leakage，最大仅约 **20 µA**。([Texas Instruments][2])

有一个边界需要明确：**它阻止的是电流经过 A3 回灌 USB 端**。如果 battery 本身在 A3 后面直接连接 SYS_5V，而 SYS_5V 本身短路，那么 A3 当然不能隔离“battery → SYS_5V short”这一条本地路径；那需要 battery path 自己的 eFuse/load switch。这两类 fault 不要混为一谈。

---

# 3. 1.5 A / 3 A：可以非常便宜地做成两个硬件模式

TPS25942 没有类似：

```text
ILIM_SEL = 0 → 1.5 A
ILIM_SEL = 1 → 3.0 A
```

这样的数字模式脚。

但是它的 `ILIM` 是**纯硬件电阻编程**，不依赖 MCU，因此很容易由你前一级 A2 的 GPIO 状态直接切换。

datasheet 的保证点包括：

|   RILIM | nominal ILIM |
| ------: | -----------: |
| 88.7 kΩ |       0.99 A |
| 42.2 kΩ |       2.08 A |
| 24.9 kΩ |       3.53 A |
| 20.0 kΩ |       4.45 A |

并且规格为大约 ±8%。([Texas Instruments][2])

因此有一个特别漂亮的两档方案：

```text
                      R1 ≈ 59 kΩ
ILIM ────────────────/\/\/\──── GND
  │
  │                   R2 ≈ 59 kΩ
  └──── NMOS ────────/\/\/\──── GND
          ▲
          │
       3A_ENABLE
```

于是：

### 1.5 A mode

NMOS OFF：

$$
R_{ILIM}\approx59kΩ
$$

得到大约：

$$
I_{LIM}\approx1.5A
$$

### 3 A mode

NMOS ON：

$$
R_{ILIM}=59k\parallel59k\approx29.5kΩ
$$

得到大约：

$$
I_{LIM}\approx3A
$$

所以只需要：

* TPS25942
* 2 × 59 kΩ resistor
* 1 × 小信号 NMOS
* 一个来自 A2 current-detection logic 的 hardware signal

就形成真正的：

```text
A2 detects 1.5 A
       │
       └── 3A_ENABLE = 0
                    ↓
                 ILIM ≈ 1.5 A

A2 detects 3 A
       │
       └── 3A_ENABLE = 1
                    ↓
                 ILIM ≈ 3 A
```

**ESP32 完全可以不参与这个 safety decision。**

不过这里有一个工程细节：TPS25942 的 ILIM accuracy 是 ±8%，所以如果“3 A mode”的意思是设备必须保证在真实 3.00 A 负载时绝不进入 current limiting，那么不能把 nominal ILIM 精确设为 3.00 A，而应留 margin，例如 3.2–3.3 A；同理 1.5 A 模式可设约 1.6 A。最终 resistor 我建议等 A2 确定后一起冻结。

---

# 4. EN default-off：需要一个外部下拉

TPS25942 的 `EN/UVLO` 是 active-high。将：

```text
EN/UVLO
   │
   ├──── A2 / safety logic enable
   │
  100k
   │
  GND
```

即可获得你要求的：

> **EN default-off**

也就是：

```text
control logic 未上电
GPIO Hi-Z
A2 未完成检测
连接器浮空
ESP32 reset
```

全部自然得到：

```text
EN = LOW
A3 = OFF
```

不需要 MCU 软件跑起来之后再禁止 power path。

我建议甚至不要让 ESP32 直接掌握唯一的 A3 enable 权限，而是类似：

```text
USB_CURRENT_VALID
        │
        ├──── hardware safety logic
        │
MCU_ALLOW
        │
        ▼
       AND
        │
        ▼
 TPS25942 EN/UVLO
```

这样 A2 没确认 source capability 时，软件无法误开启舵机电源。

---

# 5. Short protection 很合适

TPS25942 不是单纯的“慢速限流”。

它有：

1. programmable current limit；
2. **fast-trip comparator**；
3. overload thermal shutdown；
4. startup-into-short protection。

fast-trip threshold 大约是：

$$
I_{FASTRIP}=1.5I_{LIM}+0.375A
$$

当检测到快速 short transient 时，内部 FET 可在约 **1 µs** 时间尺度关闭；随后重新缓慢打开并进入 programmed current limit。如果持续产生高 dissipation，则最终 thermal shutdown。([Texas Instruments][2])

这实际上比普通 USB power-distribution switch 强很多。

---

# 6. 3 A @ TA=40°C：满足，而且余量合理

内部 FET：

$$
R_{ON,typ}=42mΩ
$$

因此 3 A continuous 时：

$$
P=I^2R
$$

$$
=3^2\times0.042
$$

$$
\approx0.378W
$$

即使按照 datasheet 在温度范围内给出的较坏 `RON = 58–64 mΩ` 来看：

$$
P_{worst}\approx9\times0.064
=0.576W
$$

器件本身规定的是 **5.2 A class**，且 electrical characterization 覆盖 1–5 A。

所以在 **TA = 40°C、3 A continuous** 下没有本质问题。

但这里有一个 PCB 条件：**WQFN exposed-pad 和 IN/OUT copper 必须按 TI layout recommendation 铺铜并打 thermal vias**。不能把它当成一个 3 × 4 mm 小逻辑 IC、只接细线。如果 PCB layout 做正确，我认为 3 A continuous 可以作为设计工作点。

---

# 7. FLT 也正好满足你的安全链

`FLT` 是：

**active-low, open-drain**

因此可以拉到你的 always-on logic supply：

```text
AON_3V3
   │
  10k
   │
   ├──────── A3_FAULT_N
   │
 TPS25942 FLT
```

可以同时送给：

* ESP32；
* TCA9539；
* hardware safety latch；
* servo power enable logic。

OVP / UVLO / thermal / overcurrent 等条件都可以通过 FLT 暴露出来。([Texas Instruments][2])

---

# 为什么不是几个更便宜的 USB switch

我还检查了几种看起来很诱人的低价器件。

例如 **Silergy SY6288E1AAC**，嘉立创有货，3 A、45 mΩ、short/thermal/reverse blocking，看起来非常接近；但它的 operating input 只有 **2.5–5.5 V**，因此直接违反你的：

> VIN ≥6 V

要求。([LCSC Electronics][3])

**Diodes AP22814BSN-7** 同样是很好的 3 A USB power switch，而且有 reverse blocking / fault / short protection，但输入也只有 **2.7–5.5 V**，同样淘汰。([LCSC Electronics][4])

Microchip `MIC2545A-1YN` 也是类似：

* 3 A；
* 35 mΩ；
* reverse blocking；
* fault；
* short protection；

但 **VIN max = 5.5 V**，因此也不符合 A3 freeze。([LCSC Electronics][5])

这就是为什么如果坚持你的 **≥6 V operating / ≥7 V absolute + true RCB** 条件，价格会从几毛钱/低价 USB switch 跳到 eFuse 这一档。

---

## TPS25940 与 TPS25942 的价格比较

你上一轮要求里我首先找到了 TPS25940，但进一步查嘉立创以后没有理由为它付更多钱：

| MPN               | LCSC        | 关键功能                   |          当前 1pcs |
| ----------------- | ----------- | ---------------------- | ---------------: |
| **TPS25942ARVCR** | **C181295** | 满足                     |      **$0.8785** |
| TPS25940LRVCR     | C2867756    | 满足                     |          $2.0065 |
| TPS25940ARVCR     | C2653873    | 满足                     | 当前页面供应情况不如 25942 |
| TPS25944ARVCR     | C140313     | 满足，但 circuit-breaker 型 |          $2.9627 |
| TPS259482AYWPR    | C22422005   | 8 A/12.2–20 mΩ，更强      |          $2.3909 |

TPS25942ARVCR 在 100 pcs 时甚至已经到 **$0.6091/pcs**。([LCSC Electronics][1])

所以在我目前检索到的**满足全部关键功能且嘉立创可买的器件中，C181295 是明显的成本最佳点**。

---

# 最终建议

我建议把 A3 初步冻结成：

**`TPS25942ARVCR / C181295 / WQFN-20-EP 3×4 mm`**

并采用下面的硬件结构：

```text
USB-C VBUS
    │
   CIN
    │
    ├──────── IN
    │      ┌────────────────┐
    └──────│ TPS25942ARVCR  │────── OUT ─── USB_5V_PROTECTED
           │                │
A2_OK ─AND─│ EN/UVLO        │
           │                │
           │ OVP ← divider  │   ≈5.8–6.0 V trip
           │                │
           │ ILIM           │── 59k ── GND
           │   │            │
           │   └─NMOS─59k───┘
           │      ▲
           │      │
           │   USB_3A_OK
           │
           │ FLT ───────────── A3_FAULT_N
           │ PGOOD ─────────── optional
           └────────────────┘
```

对应状态：

| A2 result        |                   A3 EN |      ILIM mode | Servo/high-current rail |
| ---------------- | ----------------------: | -------------: | ----------------------- |
| Default          |              视系统要求，建议禁止 | ≤ default-safe | **禁止**                  |
| 1.5 A            |                      ON |     ≈1.5/1.6 A | 受限                      |
| 3 A              |                      ON |     ≈3.0/3.2 A | 允许                      |
| A2 unknown/reset |                 **OFF** |     don't care | **禁止**                  |
| A3 fault         | OFF / hardware handling |              — | **禁止**                  |

因此，如果你的 A2 最终是 `TUSB320` 一类能够给出 Default / 1.5 A / 3 A hardware indication 的结构，那么 **A2 → A3 可以形成一条完全不依赖 ESP32 初始化的硬件电源安全链**。

**A3 我会选 `TPS25942ARVCR / C181295`。** 目前没看到为了 TPS25940/TPS25947 多花钱的理由。([LCSC Electronics][1])

[购买：嘉立创/LCSC C181295 — TPS25942ARVCR](https://www.lcsc.com/product-detail/C181295.html?utm_source=chatgpt.com)
[原厂 Datasheet：TI TPS25942A/L + TPS25944A/L Rev.D](https://www.ti.com/lit/ds/symlink/tps25942a.pdf?utm_source=chatgpt.com)

[1]: https://www.lcsc.com/product-detail/C181295.html "TPS25942ARVCR | Current Regulation/Management | TI | LCSC"
[2]: https://www.ti.com/cn/lit/ds/symlink/tps25942a.pdf "TPS25942x/44x 具有多种保护模式的 2.7V-18 V、5A 电子熔丝电源多路复用器 datasheet (Rev. D)"
[3]: https://www.lcsc.com/product-detail/Power-Distribution-Switches_Silergy-Corp-SY6288E1AAC_C207622.html?utm_source=chatgpt.com "SY6288E1AAC | Power Distribution Switches, Load Drivers | Silergy | LCSC"
[4]: https://www.lcsc.com/product-detail/C2150208.html?utm_source=chatgpt.com "AP22814BSN-7 | DIODES | Price | In Stock | LCSC Electronics"
[5]: https://www.lcsc.com/product-detail/C641208.html?utm_source=chatgpt.com "MIC2545A-1YN | Power Distribution Switches, Load Drivers | MICROCHIP | LCSC"


### A4 — Logic source mux/switch

搜索比较后，A4 现在可以比较明确地收敛了：**在“嘉立创现货 + 原厂资料能证明所有关键功能”的前提下，TPS2121RUXR 仍然是目前最低成本的完整正确方案。** 国产双 load-switch/eFuse 路线在规格上能做到，但现货成本没有打赢它。

| 方案                         | 嘉立创情况           | 关键能力                                                       |               估算核心器件成本 | 结论                           |
| -------------------------- | --------------- | ---------------------------------------------------------- | ---------------------: | ---------------------------- |
| **TPS2121RUXR / C485916**  | **1425 pcs 现货** | 双输入 mux、24 V abs、4.5 A、reverse blocking、硬件 priority        |     **$1.2128 ≈ ¥8.6** | **首选**                       |
| MT7231NQJR / C669575 ×2    | **无货**          | 24 V、28 V surge、6 A、24 mΩ、true reverse blocking、1–6 A ILIM |                   无法采购 | 技术上很好，不能冻结                   |
| LS2406ERQ23 / C5355481 ×2  | 约 91 pcs        | 24 V、30 V abs、6 A、24 mΩ、true reverse blocking、ILIM         |        **$7.16 ≈ ¥51** | 淘汰                           |
| MX74700T + MOSFET ×2       | 4.6 万 pcs       | 3.2–100 V、ideal diode、reverse-current blocking             | controller 已约 $1.08/两路 | **不满足真正双向关断**，还需更多 MOSFET/控制 |
| AP74502 + back-to-back FET | 缺货              | 3.2–80 V、双 NMOS disconnect                                 |    controller 单颗 $1.22 | 两支路明显贵于 TPS2121              |

TPS2121 当前嘉立创的实际阶梯价是：

* 1+: **$1.2128**
* 10+: **$1.0806**
* 30+: **$0.9549**
* 100+: **$0.8619**
* 库存：**1425**
* `C485916`
* `TPS2121RUXR`
* `VQFN-HR-12, 2 × 2.5 mm`

嘉立创同时明确给出了 2.8–22 V 工作、**24 V absolute maximum**、56 mΩ、双通道、reverse-current blocking，以及 1–4.5 A 可调输出限流。([LCSC Electronics][1])

[TPS2121RUXR / C485916 嘉立创商品页](https://www.lcsc.com/product-detail/ORing-Controllers_TI-TPS2121RUXR_C485916.html?utm_source=chatgpt.com)
[TPS2121 TI 原厂页面与 Datasheet](https://www.ti.com/product/TPS2121?utm_source=chatgpt.com)

### 为什么两个国产开关没有成功降价

我找到的最有意思的国产候选其实是 **M3TEK MT7231NQJR / C669575**。它几乎就是我们想要的 A4 支路开关：

* 3–24 V 工作；
* 28 V surge；
* **6 A**；
* 24 mΩ；
* **all-time true reverse current blocking**；
* 内部 common-source FET；
* 1–6 A 外部可调限流；
* EN；
* short protection；
* fault output；
* QFN 2.5 × 3.2 mm。

尤其是它明确说 reverse blocking **不依赖 EN 状态**，所以是真的适合两支路 mux，而不是普通“打开时防反流”的 load switch。([LCSC Electronics][2])

问题只有一个，但足以淘汰主方案：

**嘉立创目前无货。**

如果未来 MT7231 在嘉立创重新大量现货，而且能做到大约 **≤¥3/颗**，那么：

> MT7231 × 2 + ¥0.2–0.5 逻辑

就有可能取代 TPS2121，并且还有一个巨大优势——可以：

* USB MT7231：ILIM ≈ 3 A
* Battery MT7231：ILIM ≈ 2 A

这样恰好解决 TPS2121 的“两个通道共享 ILM”问题。

所以我建议把 **MT7231NQJR / C669575 记录为成本优化备选，而不是正式 BOM。**

### LS2406：规格漂亮，价格完全不可接受

Littelfuse `LS2406ERQ23 / C5355481` 本质上和 MT7231 是同一类器件：

* 3–24 V；
* **30 V maximum input/output**；
* 6 A；
* 24 mΩ；
* common-source internal FET；
* all-time true reverse blocking；
* adjustable current limit；
* QFN 2.5 × 3.2。

所以从技术上讲：

> LS2406(USB, 3 A) + LS2406(BATT, 2 A)

会非常漂亮。([LCSC Electronics][3])

但现在价格：

**$3.5778 / 颗。**

两颗：

**$7.16 ≈ ¥51**

还没算逻辑。

因此和 TPS2121 的约 ¥8.6 根本不是一个价格等级，直接淘汰。([LCSC Electronics][4])

---

## 为什么便宜的 ideal-diode controller 也没有获胜

我还找到了很有意思的国产 `Wuxi Maxinmicro MX74700T`。

它当前：

* `C22468156`
* 45,735 pcs 现货
* $0.5415 / 1pcs
* $0.4102 / 10pcs
* SOT-23-6
* 3.2–100 V
* 外置 NMOS
* 20 mV regulation
* reverse-current blocking。([LCSC Electronics][5])

乍看非常诱人：

> 两颗 controller 才约 ¥7.7。

然后加两颗廉价 NMOS，好像就能和 TPS2121 打。

**但不能这么用。**

MX74700 本质是 **ideal diode controller**，其标准拓扑使用一颗 NMOS。它能阻止：

> OUT → IN

的反向电流。

但这不等同于你要求的：

> **双向关断。**

单颗 MOSFET 始终有 body diode。要做到真正：

> OFF 时 IN ↔ OUT 两个方向都断开

需要 **back-to-back MOSFET** 以及能够正确控制这种拓扑的 gate controller。

这会把方案变成：

```text
MX747xx × 2
+ MOSFET × 4
+ USB presence comparator / logic
+ priority interlock
+ dead-time / BBM control
+ battery current limiting
```

这样不仅不便宜，而且验证难度突然比 TPS2121 高很多。

Diodes 的 `AP74502` 确实可以驱动 **back-to-back N-MOSFET**，3.2–80 V，关闭电流也很低，但嘉立创参考价已经是 **$1.2158/controller**，而且目前缺货。一个支路的 controller 就已经和整个 TPS2121 一样贵。([LCSC Electronics][6])

---

# 因此我建议正式确定 A4

### **A4 = TPS2121RUXR / C485916**

连接策略：

```text
USB 5 V ───────── IN1
                      \
                       TPS2121 ─── SYS_RAW ─── wide-input 3V3 buck
                      /
Battery 9–12.6 V ─ IN2
```

但不要使用它默认的“高电压优先”。

因为默认按电压的话：

```text
Battery = 9–12.6 V
USB     = 5 V
```

自然会选择 battery。

TPS2121 的优势就在于其 priority supervisor 可以**人为指定较低电压输入优先**；TI 明确说明它可以给 lower-voltage input 手动分配优先级，而不是只能 highest-voltage wins。([Texas Instruments][7])

因此设置成：

> **USB present → IN1 priority**
> **USB absent → IN2 battery**

即可。

TI 同时明确说明切换过程中采用 ideal-diode operation，在切换另一输入前控制压差，以**在反向电流发生之前将其阻断**。这比简单的两个 EN 逻辑开关更可靠。([Texas Instruments][7])

---

## 唯一需要接受的折中：battery 2 A ILIM

这是 TPS2121 **唯一真正没有完全满足你原始理想要求的地方**。

它只有：

> **一个 ILM resistor**

所以不能同时配置成：

```text
USB:
ILIM = 3 A

Battery:
ILIM = 2 A
```

TI 明确给出的 TPS2121 ILM 范围是 1–4.5 A，而且这是整个 mux 的公共 current limit。([LCSC Electronics][1])

所以我的建议是：

### A4 不负责 battery 2 A 精确限流

设置：

**TPS2121 ILM ≈ 3 A**

从而满足 USB 路径的 ≥2.5 A 要求。

battery 约 2 A 限流如果确实需要，应放到：

```text
Battery
   │
battery protection / eFuse
   │  ≈2 A
   ▼
TPS2121 IN2
```

或者如果你的 battery/BMS/前级本来就有限流，不重复增加一个昂贵器件。

这是比为了一个 2 A ILIM 把 ¥8.6 mux 拆成 ¥20–50 电源路径更合理的工程取舍。

---

# A4 当前冻结建议

**MPN:** `TPS2121RUXR`
**LCSC:** `C485916`
**Package:** `VQFN-HR-12, 2 × 2.5 mm`
**VIN operating:** 2.8–22 V
**VIN abs max:** **24 V**
**Imax:** 4.5 A
**RON:** 56 mΩ typ
**USB priority:** hardware PR1 supervisor
**Reverse blocking:** yes
**MCU required:** **no**
**Output:** → wide-input 3V3 buck
**ILM:** 暂定约 **3 A**

单颗当前价格：

**约 $1.21 ≈ ¥8–9。** ([LCSC Electronics][1])

因此我会把结论定为：

> **目前没有找到嘉立创可采购、资料可信、完整满足安全要求，并且完整 BOM 明显低于 TPS2121 的国产/分立方案。TPS2121RUXR / C485916 是当前 A4 的最低成本可信解。**
>
> **MT7231NQJR / C669575 是唯一值得持续关注的潜在降本替代；一旦恢复现货且价格足够低，两颗 MT7231 会因为可分别设置 USB 3 A / battery 2 A ILIM 而比 TPS2121 更完整。**

这里我认为已经足够把 **A4 主器件冻结为 TPS2121RUXR/C485916**，而不必继续为了可能省一两元引入四颗 MOSFET + controller + interlock 的复杂分立路径。

[1]: https://www.lcsc.com/product-detail/ORing-Controllers_TI-TPS2121RUXR_C485916.html "TPS2121RUXR | OR Controllers, Ideal Diodes | TI | LCSC"
[2]: https://www.lcsc.com/product-detail/Motor-Driver-ICs_M3TEK-MT7231NQJR_C669575.html?utm_source=chatgpt.com "MT7231NQJR | Power Distribution Switches, Load Drivers | M3TEK | LCSC"
[3]: https://www.lcsc.com/product-detail/C5355481.html?utm_source=chatgpt.com "LS2406ERQ23 | Current Regulation/Management | Littelfuse | LCSC"
[4]: https://www.lcsc.com/product-detail/C5355481.html "LS2406ERQ23 | Current Regulation/Management | Littelfuse | LCSC"
[5]: https://www.lcsc.com/zh-CN/product-detail/C22468156.html?utm_source=chatgpt.com "MX74700T | OR Controllers, Ideal Diodes | Wuxi Maxinmicro | LCSC"
[6]: https://www.lcsc.com/zh-CN/product-detail/C48984699.html?utm_source=chatgpt.com "AP74502QTA8-7 | OR Controllers, Ideal Diodes | DIODES | LCSC"
[7]: https://www.ti.com/product/TPS2121?utm_source=chatgpt.com "TPS2121 data sheet, product information and support | TI.com"


## B. SY8388 passives

### B1 — 2.7 µH power inductor

## B1 功率电感选型问题总结

### 设计目标

当前 B1 是主降压电源的功率电感，原始目标规格为：

* 电感值：**2.7 µH ±20%**
* 结构：**shielded power inductor**
* 原先写的是 ferrite，但如果一体成型 metal-composite 在电气性能上更合适，可以讨论放宽材料要求
* **Isat ≥12 A**
* **Irms / temperature-rise current ≥8.5 A**
* **DCR ≤8 mΩ**
* 制造商资料必须明确：

  * Isat 对应的电感下降判据，例如 ΔL/L = 20% 或 30%
  * Irms 对应的温升条件，例如 ΔT = 40°C
* 优先尺寸：**7×7 到 10×10 mm**
* 采购约束：**国产、嘉立创可采购、目标单价约 ¥5/颗**

### 当前遇到的核心矛盾

严格坚持 **2.7 µH + DCR≤8 mΩ + Isat≥12 A + Irms≥8.5 A + 10 mm以内 + 国产约¥5** 时，候选非常少。

目前找到的很多 2.7 µH 国产一体成型电感虽然电流能力够，但 DCR 普遍在 **12–16 mΩ** 左右。

例如某些 7–10 mm 级 2.7 µH 型号可以做到：

* Isat >12 A
* Irms >8.5 A

但 DCR 常在：

**12–15 mΩ**

这会显著增加 8 A 级 buck 的铜耗和温升。

因此目前考虑的优化方向是：

> 是否允许将名义电感从 2.7 µH 改为 2.2 µH，以换取明显更低的 DCR 和更好的电流余量。

---

## 当前重点候选：Sunlord MWSA1004S-2R2MT

顺络 `MWSA1004S-2R2MT` 是目前最有吸引力的国产候选之一。

主要参数：

* **L = 2.2 µH ±20%**
* **DCR max ≈7 mΩ**
* **Isat guaranteed ≈13.2 A**
* Isat typ ≈16.5 A
* **Irms guaranteed ≈10 A**
* Irms typ ≈12 A
* 约 **10×10 mm class**
* 一体成型、磁屏蔽
* 国产
* 成本预计可以接近目标范围

因此它除了名义电感值不是 2.7 µH 外，基本满足原 B1 的核心电气要求：

| 项目   |         原要求 |        MWSA1004S-2R2MT |
| ---- | ----------: | ---------------------: |
| L    | 2.7 µH ±20% |        **2.2 µH ±20%** |
| DCR  |       ≤8 mΩ |          **≈7 mΩ max** |
| Isat |       ≥12 A | **≈13.2 A guaranteed** |
| Irms |      ≥8.5 A |   **≈10 A guaranteed** |
| 尺寸   |     ≤约10 mm |                 **满足** |
| 国产   |           是 |                  **是** |

需要注意：MWSA1004S 属于一体成型 metal-material / composite 类功率电感，不应简单称为传统 ferrite-core。如果“ferrite”本身是硬性材料约束，则需要重新评价；如果真正关心的是磁屏蔽、低 DCR、高饱和和大电流能力，则这种材料通常反而更适合。

---

## 为什么考虑从 2.7 µH 改成 2.2 µH

相同工作条件下，buck 电感纹波近似满足：

$$
\Delta I_L \propto \frac{1}{L}
$$

因此从 2.7 µH 改为 2.2 µH，会使纹波电流增加约：

$$
\frac{2.7}{2.2}\approx1.23
$$

即大约 **增加 23%**。

这会带来：

* 电感峰值电流增加；
* MOSFET RMS 电流略增加；
* 输入/输出电容纹波增加；
* EMI 可能略变差。

因此不能仅凭电感自身参数优秀就直接替换，必须结合主 buck 芯片的实际工作条件计算。

另一方面，2.2 µH 方案的 DCR 优势明显。

以 8.5 A 为例：

若 DCR = 7 mΩ：

$$
P=I^2R
=8.5^2\times0.007
\approx0.51W
$$

如果使用某个 DCR = 12 mΩ 的 2.7 µH 电感：

$$
P\approx8.5^2\times0.012
\approx0.87W
$$

二者仅 DC copper loss 就相差约：

$$
0.36W
$$

对于紧凑型、高电流 buck，这个差别很有价值。

---

## 当前需要外部评价的核心问题

希望重点判断下面几个问题：

1. **对于当前 buck，2.7 µH 是否实际上是硬要求？**

   如果原设计只是按照推荐值选择 2.7 µH，而不是控制环路或 minimum inductance 的硬约束，那么改用 2.2 µH 可能更合理。

2. **2.2 µH 导致约 23% 的纹波增加是否仍在合理范围？**

   需要结合：

   * Vin min/max
   * Vout
   * 最大连续输出电流
   * switching frequency
   * buck 芯片 current limit

   重新计算 ΔIL 和 Ipeak。

3. **MWSA1004S-2R2MT 的 Isat 判据是否足够保守？**

   希望核实顺络 datasheet 中 Isat 的具体定义，例如：

   * L drop 20%？
   * L drop 30%？

   并确认在实际峰值电流下仍有足够 margin。

4. **Irms 的温升定义是什么？**

   需要确认顺络给出的约 10 A guaranteed Irms 对应：

   * ΔT = 40°C？
   * 其他条件？

   以及测试 PCB / ambient 条件。

5. **是否应该坚持 ferrite 材料要求？**

   当前更高性能、更低 DCR 的国产大电流电感主要是一体成型 metal-composite。

   如果系统层面没有必须使用 ferrite 的特殊 EMI / core-loss 原因，可以考虑把 B1 规格从：

   > shielded ferrite

   改成：

   > magnetically shielded molded / composite power inductor

6. **成本和面积权衡是否合理？**

   目前目标是国产、约 ¥5、10 mm 级。

   如果为了坚持 2.7 µH 必须接受 12–15 mΩ DCR，我倾向认为不如改用性能明显更好的 2.2 µH。

---

## 当前倾向

当前倾向是：

> **如果主 buck 在全部 Vin/Vout/load 条件下使用 2.2 µH 后，纹波、峰值电流、current-limit margin 和控制稳定性均满足要求，则优先选择 MWSA1004S-2R2MT，而不是强行坚持 2.7 µH。**

原因是它同时获得：

* 国产
* 成本可控
* 约 10 mm 尺寸
* DCR ≤8 mΩ
* Isat >12 A
* Irms >8.5 A

而目前找到的 2.7 µH 国产小尺寸方案往往需要明显牺牲 DCR。

因此现在真正需要判断的不是“2.2 µH 是否和 2.7 µH 一样”，而是：

> **在具体 buck 工作点下，2.2 µH 增加的纹波代价，是否小于它在 DCR、热性能、电流余量和成本上的收益。**


### 2026-08-31 独立复算处置

按`VOUT=6.0 V`、`VIN=9.0/12.6 V`、`fSW,min=510 kHz`及`±20%`电感容差复算，最坏点为`VIN=12.6 V`和`Lmin`：

| Nominal L | Lmin | Worst ripple | Ripple / 8 A | 8 A peak | 6 A RMS | 8 A RMS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 2.2 µH | 1.76 µH | 3.501 A | 43.77% | 9.751 A | 6.085 A | 8.064 A |
| 2.7 µH | 2.16 µH | 2.853 A | 35.66% | 9.426 A | 6.056 A | 8.042 A |

AN_SY8388A Rev.0.9建议满载纹波约20–40%；因此2.2 µH名义点虽可接受，其最坏容差角落超过40%，不能按当前证据直接替换EDD-024已选的2.7 µH。仓库也没有`MWSA1004S-2R2MT`原厂资料；本节早先写出的约7 mΩ/13.2 A/10 A仍是待核实候选数据，不是已验证事实。

当前处置：保持`2.7 µH ±20%`为基线，优先寻找满足DCR/Isat/Irms/热条件的exact 2.7 µH MPN。MWSA可保留为低DCR实验候选，但只有取得原厂Isat下降判据、Irms温升条件、DCR maximum/temperature资料，并完成current-limit、AC/core loss、输出纹波、EMI和稳定性复核后，才能提出修改EDD-024。

### B2 — SY8388 input capacitors

我建议把 **B2 先冻结为 `4 × Samsung CL32B226KAJNNNE` 并联**，而不是用风华 1210 22 µF 直接拍脑袋按 nominal 计算。

原因不是三星“品牌更高级”，而是它恰好满足你这次最重要的要求：**原厂页面直接提供这颗 exact MPN 的 DC-bias、Bias-TCC、ESR/|Z| 和 ripple-current self-heating 曲线**。([product.samsungsem.com][1])

| 项目            | 选择                              |
| ------------- | ------------------------------- |
| Exact MPN     | **CL32B226KAJNNNE**             |
| 厂商            | Samsung Electro-Mechanics       |
| LCSC/JLC      | **C309062**                     |
| 单颗            | **22 µF ±10%, 25 V, X7R, 1210** |
| 数量            | **4 并联**                        |
| Nominal total | **88 µF**                       |
| 封装            | 1210 / 3225                     |
| 温度范围          | −55…125 °C                      |
| 嘉立创库存         | 约十万颗级，库存充足                      |
| 嘉立创价格         | 当前搜索结果约 **US$0.13/pcs 起**，批量更低  |

嘉立创明确列出了 `C309062 = CL32B226KAJNNNE`, 22 µF、25 V、X7R、±10%、1210，并且当前库存很充足。([LCSC Electronics][2])

[嘉立创/LCSC：CL32B226KAJNNNE / C309062](https://www.lcsc.com/product-detail/Samsung-Electro-Mechanics-CL32B226KAJNNNE_C309062.html?utm_source=chatgpt.com)

[三星原厂 exact-MPN characteristic page](https://product.samsungsem.com/mlcc/CL32B226KAJNNN.do?utm_source=chatgpt.com)

### 为什么我选 4 颗，而不是 2 颗或 3 颗

最差输入是约 **12.6 V DC bias**，也就是这颗 25 V MLCC 工作在额定电压的约：

$$
12.6/25=50.4\%
$$

这里绝对不能把：

$$
4\times22=88\mu F
$$

直接当成 88 µF。

三星恰好给这颗器件提供了 **DC Bias Characteristics**，测试条件明确为 0.12 kHz、0.5 Vrms；更重要的是还单独提供了 **Bias TCC Characteristics at 12.5 Vdc**，和你要求的 12.6 V 工况几乎完全吻合。([product.samsungsem.com][1])

因此 B2 的判断应该采用：

$$
C_\text{eff}
=
N C_\text{nom}
\times K_\text{DCbias}
\times K_\text{tol}
\times K_\text{temp}
$$

而不是 nominal 容量。

你要求：

$$
C_{\text{eff,total}}\ge40\mu F
$$

对于四颗 22 µF：

$$
C_{nom,total}=88\mu F
$$

公差先取最低：

$$
K_{tol}=0.9
$$

于是即便把 DC bias + 40°C 温度影响合在一起非常保守地只允许：

$$
K_{bias+temp}
=
\frac{40}{88\times0.9}
\approx0.505
$$

也就是说，只要三星原厂 **12.5 V Bias-TCC 曲线在 40°C 时保留 nominal 的约 50.5% 以上**，四颗方案就通过。

这就是四颗方案很有意义的地方：它不是要求 MLCC 在半额定电压下仍保留 80–90%，只需要约一半。

相比之下，三颗只有：

$$
66\mu F
$$

扣掉 −10% tolerance 后：

$$
59.4\mu F
$$

要保证 40 µF，就要求 bias+temperature retention：

$$
40/59.4
\approx67.3\%
$$

对于 **22 µF / 25 V / 1210 Class-II MLCC @ 12.6 V**，这个要求太激进，我不会在没有逐点读出厂商曲线的情况下冻结。

所以：

**3×22 µF：不建议。**

**4×22 µF：合理的设计起点。**

---

### 纹波电流方面，四颗也比“单颗额定 4 A”这种思路正确

三星这颗 exact MPN 的原厂页面提供：

* ESR vs frequency；
* impedance；
* **Temperature Rise by Ripple Current**；
* 分别在 **10 kHz / 100 kHz / 500 kHz / 1 MHz** 下给出自热数据。([product.samsungsem.com][1])

这正是验证 MLCC ripple capability 应该看的东西。

对四颗相同 MLCC，如果 PCB 对称布置且 ESR/ESL 接近，4 A RMS 总纹波近似平均分担：

$$
I_{rms,cap}\approx\frac{4A}{4}=1A
$$

每颗只承担约 **1 Arms**。

而高品质 1210 MLCC 在数百 kHz 这一档 ESR 通常是毫欧量级，所以即使粗略按：

$$
ESR=5m\Omega
$$

计算，每颗 1 Arms 的损耗也只有：

$$
P=I^2R
=1^2\times0.005
=5mW
$$

四颗合计约 20 mW。

实际验证时应以 **SY8388 的实际 switching frequency 对应的三星 Temperature Rise by Ripple Current 曲线**为准；但从负载分配来看，**4×1210 承担总计 4 Arms 并不是一个苛刻工况**。

这里我不会给你写“单颗 rated ripple = 2 A”之类的数字，因为三星没有这么定义。它提供的是更有意义的 **ripple-current → temperature-rise characteristic**。([product.samsungsem.com][1])

---

## 为什么这次我暂时不冻结国产风华

风华确实有很诱人的：

**`1210B226K250NT / C5204920`**

参数也是：

* 22 µF
* ±10%
* 25 V
* X7R
* 1210

而且属于国产，嘉立创可采购。([LCSC Electronics][3])

问题在于：虽然风华的 exact-product 页面有一个 **Characteristic Data** 区域，但目前公开检索到的页面没有把图表数值可靠地暴露出来；嘉立创 datasheet 也主要证明 nominal specification，而不能让我可靠地读出：

$$
C_\text{eff}@12.6V,\ 40^\circ C
$$

以及：

$$
\Delta T @ I_{ripple}
$$

数值。([fhcomp.com][4])

而你的要求已经明确说了：

> 不能只报 nominal µF。

所以在拿不到可审计的 curve data 时，我宁可**不声称风华通过**。

---

## 成本其实并不高

三星 `C309062` 在嘉立创当前属于非常常见的库存料，搜索结果显示单价大约 US$0.13 起，100 pcs 档甚至约 US$0.085/pcs。([LCSC Electronics][2])

按照这个价格粗略看：

$$
4\times \$0.13
\approx \$0.52
$$

大约也就是几元人民币一整组。

因此这里**没有为了省一两元而放弃可验证 DC-bias/ripple 数据的必要**。

---

## 我建议的 B2 电路

```text
BAT / VIN 9–12.6 V
       │
       ├── C1 22uF / 25V / X7R / 1210
       │
       ├── C2 22uF / 25V / X7R / 1210
       │
       ├── C3 22uF / 25V / X7R / 1210
       │
       ├── C4 22uF / 25V / X7R / 1210
       │
       └── 0.1uF~1uF HF ceramic
       │
    SY8388 VIN/PGND
```

四颗 **不要排成很长的一条链**。应尽量让它们直接跨接在 SY8388 的 VIN power plane 和 PGND plane 之间，尤其至少让其中两颗紧贴高侧 MOSFET 的输入高 di/dt loop。

### 当前冻结建议

我会写成：

> **B2 — `4 × CL32B226KAJNNNE (Samsung, C309062), 22 µF ±10%, 25 V, X7R, 1210, in parallel`**
>
> Nominal = 88 µF.
> Final acceptance criterion: use Samsung exact-MPN **DC Bias / Bias-TCC at 12.5 Vdc** data to demonstrate \(C_\mathrm{eff,total}\ge40\,µF\) at 12.6 V, 40°C including −10% capacitance tolerance; use exact-MPN **Temperature Rise by Ripple Current** data at the actual SY8388 switching frequency to demonstrate ≥4 Arms combined capability. ([product.samsungsem.com][1])

**这比“4×22 µF，所以有88 µF”严格得多，也符合你 B2 的证据要求。**

不过还有最后一个我不想伪造的数字：网页搜索已经确认三星确实有这两个 exact-MPN 曲线，但当前文本抓取没有返回曲线上的具体 ordinate 数据。因此，**我现在可以确定器件和 4 颗拓扑，但不会假装已经从图上读出了 12.6 V/40°C 的 exact µF**。正式 Design Freeze 前，需要把三星生成的 PDF/曲线本身存档并逐点读出这个数值；如果读出来低于约 50.5% retention，则直接升级到 **5×22 µF**，不会改变料号。([product.samsungsem.com][5])

[1]: https://product.samsungsem.com/mlcc/CL32B226KAJNNN.do?utm_source=chatgpt.com "CL32B226KAJNNN# | MLCC | SAMSUNG ELECTRO-MECHANICS"
[2]: https://lcsc.com/product-detail/Samsung-Electro-Mechanics-CL32B226KAJNNNE_C309062.html?utm_source=chatgpt.com "CL32B226KAJNNNE | Samsung Electro-Mechanics | Price | In Stock | LCSC Electronics"
[3]: https://www.lcsc.com/ko/product-detail/C5204920.html?utm_source=chatgpt.com "1210B226K250NT | Ceramic Capacitors | FH | LCSC"
[4]: https://fhcomp.com/en/product_info/?prod_code=1210X226K250NT&utm_source=chatgpt.com "1210X226K250NT | General MLCC-FengHua"
[5]: https://product.samsungsem.com/cn/mlcc/CL32B226KAJNNN.do?utm_source=chatgpt.com "CL32B226KAJNNN# | 陶瓷电容 | 产品检索"


### B3 — SY8388 output capacitors

我建议把 **SY8388ARHC 的输出电容**改成：

**`2 × JIERR PB16V330M6X11 + 2 × 22 µF/25 V X7R MLCC + 0.1 µF`**

而不是从 `2×220 µF` 起步。这个方案在成本、有效容量、ESR 和 8 A 级瞬态承受能力之间更合适。

| 项目                             | 选型                                 |
| ------------------------------ | ---------------------------------- |
| 主输出电容                          | **2 × PB16V330M6X11**              |
| 厂商                             | JIERR                              |
| LCSC                           | **C51934161**                      |
| 单颗                             | 330 µF ±20%, **16 V polymer**      |
| ESR                            | **16 mΩ @100 kHz / pcs**           |
| Ripple                         | **4.1 A @100 kHz / pcs**           |
| 寿命                             | 5000 h @105°C                      |
| 封装                             | D6.3 × 11 mm，插件                    |
| 高频陶瓷                           | 建议 **2 × 22 µF/25 V X7R + 0.1 µF** |
| 总 nominal                      | **704 µF + 0.1 µF**                |
| Polymer worst tolerance        | **528 µF**                         |
| Polymer combined ESR           | **8 mΩ**                           |
| Polymer combined ripple rating | **8.2 A @100 kHz**                 |

嘉立创当前约有 2800 颗库存，5 pcs 档约 **US$0.158/颗**，50 pcs 档约 US$0.124/颗，因此两颗主电容成本很低。([LCSC Electronics][1])

[嘉立创/LCSC：PB16V330M6X11 / C51934161](https://www.lcsc.com/product-detail/C51934161.html?utm_source=chatgpt.com)

## 为什么我选 2×330 µF，而不是 2×220 µF

我们找到的廉价国产 220 µF/16 V polymer，例如 JIERR `PB16V220M6X8`，单颗是：

* 220 µF ±20%
* 16 V
* ESR **20 mΩ**
* ripple **3.5 A @100 kHz**。([LCSC Electronics][2])

两颗并联：

$$
C_{nom}=440\mu F
$$

最低公差：

$$
C_{min}=440\times0.8=352\mu F
$$

容量刚刚超过你的：

$$
C_{eff}\ge330\mu F
$$

但余量只有：

$$
22\mu F
$$

而且：

$$
ESR_{eq}=\frac{20}{2}=10m\Omega
$$

也是**刚好踩线**。

更重要的是 ripple：

$$
I_{ripple,total}=3.5\times2=7A
$$

没有覆盖你的 **8 A** 要求。([LCSC Electronics][2])

所以 `2×220 µF` 对你的新规格而言实际上是：

**容量踩线 + ESR踩线 + ripple FAIL。**

不值得冻结。

---

# 2×330 µF 则同时解决三个问题

`PB16V330M6X11` 单颗：

$$
330\mu F,\quad ESR=16m\Omega,\quad I_{ripple}=4.1A
$$

两颗并联以后：

### 容量

$$
C_{nom}=660\mu F
$$

考虑 −20% tolerance：

$$
C_{min}=660\times0.8
=528\mu F
$$

所以即使**完全不把陶瓷算入 330 µF 指标**：

$$
528\mu F>330\mu F
$$

有近 60% 的余量。

聚合物铝电容没有 Class-II MLCC 那种严重的 DC-bias capacitance collapse，因此在 6 V 工作点，不需要像 10 V X5R MLCC 一样打五折甚至更低。这里用 16 V polymer 工作在 6 V，电压余量也很舒服。器件额定工作温度到 105°C，你的 40°C 环境并不苛刻。([LCSC Electronics][1])

### ESR

两颗相同电容近似：

$$
ESR_{eq}=\frac{16m\Omega}{2}
=\boxed{8m\Omega}
$$

满足：

$$
ESR\le10m\Omega
$$

而旁边再放两颗 MLCC 后，高频等效阻抗还会进一步下降。

### Ripple

$$
I_{ripple,total}
=2\times4.1
=\boxed{8.2A@100kHz}
$$

因此即使把你写的“覆盖 8 A transient”按更严苛的**8 Arms 连续 ripple capability**去理解，这两颗仍然在规格线上方。([LCSC Electronics][1])

实际 SY8388A 是约 **600 kHz** switching，矽力杰官方给出的工作频率也是约 600 kHz；你的“8 A transient”也并不等于电容长期承担 8 Arms，因此这里实际上还有额外裕量。

---

# 本地 ceramics 我建议继续用 22 µF/25 V

可以直接复用前面 B2 的：

**Samsung `CL32B226KAJNNNE` / C309062**

即：

$$
2\times22\mu F/25V/X7R/1210
$$

再加：

$$
0.1\mu F
$$

紧贴 SY8388 输出侧。

这样 BOM 还能复用同一个 MLCC 料号。

结构建议：

```text
                 L
SY8388 LX ─────UUUU────── VOUT 6V
                         │
                         ├── 22uF / 25V X7R
                         ├── 22uF / 25V X7R
                         ├── 0.1uF ceramic
                         │
                         ├── 330uF / 16V polymer
                         └── 330uF / 16V polymer
```

这里我建议：

* **22 µF ceramics 靠近电感输出和 IC GND**
* 两颗 polymer 紧跟其后
* 两颗 polymer 尽量采用宽铜面、近似等阻抗连接

避免一颗离芯片很近、一颗通过很细长的 VOUT trace 连接，否则 8 mΩ 的理论并联 ESR在高频瞬态时不会完全兑现。

---

## 为什么 local ceramics 不能替代 polymer

SY8388A 的原厂 application note 对这件事情说得很清楚。

矽力杰说明 output ripple 同时来自：

$$
V_{ripple,ESR}=\Delta I_L\times ESR
$$

以及：

$$
V_{ripple,C}
=
\frac{\Delta I_L}{8C_{OUT}f_{SW}}
$$

并指出陶瓷电容在施加电压后实际 capacitance 会下降，因此实际 capacitive ripple 可能高于按 nominal C 算出的结果。

它还专门讨论 fast load transient：输出瞬态不仅取决于 ESR，还取决于输出电容储能和电感电流的 slew rate。

所以你这里采用：

**大容量 polymer 提供 energy reservoir + MLCC 接高频**

是正确方向。

---

# 但是必须启用大 COUT 的 RFF/CFF 设置

这一点非常关键，因为你明确写了：

> 如果 effective/nominal total ≥500 µF，需保留 datasheet 推荐 RFF/CFF 补偿。

这个方案：

$$
C_{nom,poly}=660\mu F
$$

即使 tolerance 最低：

$$
C_{poly,min}=528\mu F
$$

所以已经明确进入：

$$
C_{OUT}>500\mu F
$$

区域。

矽力杰原厂 application note 明确写道：

当：

$$
C_{OUT}>500\mu F
$$

且 minimum load current 较低时，应使用：

$$
\boxed{R_{FF}=1k\Omega}
$$

$$
\boxed{C_{FF}=2.2nF}
$$

以给 FF pin 提供足够 ripple，并获得良好的 transient behavior。

因此这个值我建议**直接纳入设计，不要 DNP**：

```text
VOUT
 │
 ├──── RFF = 1 kΩ ────┐
 │                    │
 └──── CFF = 2.2 nF ──┴── FF
```

更准确地说，是按照 SY8388A datasheet/AN 所示 RC feed-forward topology 接到 FF/FB 相应节点，而不是只把这两个元件留 footprint。

顺带一提，SY8388A 的普通参考设计通常使用类似：

$$
R_{FF}=1k\Omega,\quad C_{FF}\approx220\text{–}470pF
$$

但大容量输出时要提高到 **2.2 nF**。官方资料明确说明了这个区别。([LCSC 数据表][3])

---

# 8 A load step 时的 ESR 瞬态

如果最极端按照：

$$
\Delta I=8A
$$

瞬间阶跃计算，仅 ESR 项：

$$
\Delta V_{ESR}
=
8A\times8m\Omega
=
\boxed{64mV}
$$

这是只算两颗 polymer 的情况。

实际在快速边沿，紧贴负载和电感的 MLCC 会承担相当大一部分初始电流，因此实际最高频段等效阻抗会低于 8 mΩ。

对 6 V rail：

$$
\frac{64mV}{6V}\approx1.07\%
$$

单看 ESR 项是合理的。

而且相比 datasheet 举例中的单颗 40 mΩ polymer：

$$
8A\times40m\Omega=320mV
$$

你的 8 mΩ bank 已经低很多。

---

# 我不建议用 AISHI 那颗 SMD 330 µF

我也检查了国产 SMD 方案。

例如：

`AISHI SVZ1CM331E09E00RAXXX / C3291468`

虽然：

* 330 µF
* 16 V
* SMD D6.3×9 mm
* 价格也便宜

但原始数据是：

$$
ESR=25m\Omega
$$

$$
I_{ripple}=2.6A
$$

两颗只能得到：

$$
ESR_{eq}=12.5m\Omega
$$

以及：

$$
I_{ripple,total}=5.2A
$$

两项都 FAIL。([LCSC Electronics][4])

所以如果没有“必须全 SMD”要求，**不要为了贴片化而牺牲性能**。

---

# 最终 B3/B2-output freeze 建议

我会把这部分正式写成：

> **SY8388 Output Capacitor Bank**
>
> **2 × JIERR PB16V330M6X11 / C51934161**
> 330 µF ±20%, 16 V polymer, ESR ≤16 mΩ @100 kHz, ripple ≥4.1 A @100 kHz, D6.3×11 mm.
>
> Parallel bank:
>
> $$
> C_{nom}=660\mu F
> $$
>
> $$
> C_{min,tol}=528\mu F
> $$
>
> $$
> ESR_{eq}\le8m\Omega
> $$
>
> $$
> I_{ripple,total}\ge8.2A
> $$
>
> Add **2 × 22 µF/25 V X7R + 0.1 µF ceramic locally**.
>
> Because \(C_{OUT}>500µF\), **retain and populate**
>
> $$
> \boxed{R_{FF}=1k\Omega,\quad C_{FF}=2.2nF}
> $$
>
> per Silergy recommendation. ([LCSC Electronics][1])

**所以我的选择不是 2×220 µF，而是直接上 `2×330 µF/16 V polymer`。** 成本增加很小，却把容量、ESR 和 8 A ripple 三个指标从“踩线/失败”一起变成 PASS。

[1]: https://www.lcsc.com/product-detail/C51934161.html?utm_source=chatgpt.com "PB16V330M6X11 | Aluminum - Polymer Capacitors | JIERR | LCSC"
[2]: https://www.lcsc.com/product-detail/C51934159.html?utm_source=chatgpt.com "PB16V220M6X8 | JIERR | Price | In Stock | LCSC Electronics"
[3]: https://datasheet.lcsc.com/lcsc/2207201830_Silergy-Corp-SY8388ARHC_C5110279.pdf?utm_source=chatgpt.com "Application Note: AN_ SY8388A"
[4]: https://www.lcsc.com/zh-CN/product-detail/C3291468.html "SVZ1CM331E09E00RAXXX | Aluminum - Polymer Capacitors | AISHI | LCSC"

## C. Servo source and measurement

### C1 — 6 V/5 V source isolation switch/controller

我会选**分立背靠背 P-MOSFET 方案**，而不是两颗 eFuse。按你的“成本第一”原则，这里分立方案有明显优势，而且电气上能满足“OFF 双向隔离、默认关断、故障源不被另一源持续供能”。

推荐两条路径都用同一颗 MOSFET：

**AONS21357-HXY / LCSC C22367089，DFN5×6，30 V P-channel，2 颗/路径，共 4 颗。**

嘉立创当前 1+ 约 **$0.35/颗**，10+ 约 $0.275；因此四颗功率 MOSFET 只约 **$1.4 ≈ ¥10**，再加两个小信号 NPN/NMOS 和若干电阻，完整 isolation stage 仍然大致在 **¥11–13**。([LCSC Electronics][1])

[AONS21357-HXY / C22367089 嘉立创商品页](https://www.lcsc.com/zh-CN/product-detail/mosfets_hxy-mosfet-aons21357-hxy_C22367089.html?utm_source=chatgpt.com)
[AOS AONS21357 原厂 Datasheet](https://www.aosmd.com/sites/default/files/res/datasheets/AONS21357.pdf?utm_source=chatgpt.com)

### 推荐拓扑

每个 source 使用两颗 **source-to-source 背靠背 P-MOSFET**：

```text
6V_SOURCE
   |
  D
 Q1 P-MOS
  S
  +------ common-source node
  S
 Q2 P-MOS
  D
   |
SERVO_POWER_BUS
```

USB 5 V 路径完全一样：

```text
USB_5V
   |
  D
 Q3
  S
  +------ common-source node
  S
 Q4
  D
   |
SERVO_POWER_BUS
```

每对 MOSFET：

```text
Gate ---- 100k ---- common-source
                  ↑
             default OFF

Gate ---- small NPN/NMOS ---- GND
                  ↑
                 EN
```

因此如果控制逻辑掉电，gate 自动被拉回 source：

**VGS ≈ 0 → 两颗 P-MOS 都 OFF。**

这满足你的 **EN default-off**。

---

## 为什么我选 AONS21357

AOS 原厂规格比嘉立创页面上的简化参数更有价值：

* `VDS = -30 V`
* `RDS(on) ≤ 7.8 mΩ @ VGS=-10 V`
* **`RDS(on) ≤ 12.3 mΩ @ VGS=-4.5 V`**
* `ID = 36 A @ TC=25°C`
* `TJ max = 150°C`
* `RθJA ≤55 °C/W`，按原厂指定 PCB 测试条件
* `IDSS ≤1 µA @ 25°C`
* `IDSS ≤5 µA @ TJ=55°C`

因此它不是那种“额定 50 A，但 5 V gate 根本导不透”的 MOSFET；**4.5 V 的 RDS(on) 有原厂保证。** ([Alpha & Omega Semiconductor][2])

这对你的 5 V/6 V gate-drive 场景很重要。

### 6 V / 6 A 连续路径

最保守用：

`RDS(on)=12.3 mΩ/FET`

两颗串联：

`RPATH ≤ 24.6 mΩ`

于是 6 A：

`P = I²R = 36 × 0.0246 ≈ 0.886 W`

也就是每颗约：

`0.443 W`

按 `55 °C/W` 的保守自然散热估算：

`ΔT ≈ 24°C/FET`

环境 40°C：

`TJ ≈ 64°C`

实际上 PCB 铜面积做得合适即可，离 150°C 还有很大余量。原厂给出的 DFN5×6 参数和热性能也支持这一判断。([Alpha & Omega Semiconductor][3])

所以：

**6 A continuous：PASS**

### 8 A / 1 s

同样用最坏 12.3 mΩ：

`Ppath ≈ 64 × 0.0246 = 1.57 W`

每颗：

`≈0.79 W`

而且只有 1 秒。AOS 数据手册明确提供 single-pulse thermal impedance/SOA 数据；以这个工作点而言远低于器件能力。

所以：

**8 A / 1 s：PASS，余量充分。**

---

## USB 5 V / 3 A

仍然使用相同 MOSFET，可以直接减少 BOM variety。

两颗总最大约：

`24.6 mΩ`

3 A：

`P ≈ 9 × 0.0246 = 0.221 W`

每颗：

**≈110 mW**

基本没有热问题。

所以：

**USB 3 A continuous：PASS**

---

# OFF reverse leakage

这是你这次特别要求的指标。

AOS 对 AONS21357 给出的 zero-gate drain leakage：

* 25°C：**≤1 µA**
* 55°C：**≤5 µA**

而你的工作环境是约 40°C。

即便非常保守地考虑：

* 两颗 MOS；
* gate 驱动漏电；
* PCB surface leakage；

做到：

**OFF reverse leakage <100 µA**

没有压力。

所以你的目标不是“勉强满足”，而是理论上可以有一个数量级以上余量。

---

# 最关键：source short 时不会反灌

这就是为什么一定要用 **back-to-back**。

普通单颗 MOSFET：

```text
GOOD_SOURCE --> BUS --> body diode --> FAILED_SOURCE
```

即使 gate OFF，也可能经 body diode 给故障电源持续供能。

两颗反向串联后：

```text
        |< body diode
----MOS1----MOS2----
                 body diode >|
```

两个 body diode 方向相反。

因此 OFF 时无论：

```text
SOURCE > BUS
```

还是：

```text
BUS > SOURCE
```

都没有持续的 body-diode conduction path。

TI 对 back-to-back MOSFET 的描述也是如此：dual/back-to-back FET 在关闭时能**双向阻断电流**。([Texas Instruments][4])

因此假如：

```text
6V converter output -> short to GND
```

而系统已经切换成 USB：

```text
USB 5V
 ↓
USB B2B switch ON
 ↓
SERVO_BUS
 ↓
6V B2B switch OFF
 X
shorted 6V converter
```

USB 不会通过 6 V source path 持续向短路供电。

反过来也成立。

这正好满足你的：

> faulted source short时另一source不得持续供能。

---

# 互斥逻辑必须这样做

这里我不建议：

```text
EN_6V  = MCU_GPIO1
EN_USB = MCU_GPIO2
```

因为软件 bug 可以同时拉高。

应该建立一个**硬件 one-hot interlock**。

例如系统只生成一个：

```text
SERVO_SOURCE_SEL
```

然后：

```text
USB_SW_EN = USB_ALLOWED & !SERVO_SOURCE_SEL
6V_SW_EN  = SYS_ALLOWED &  SERVO_SOURCE_SEL
```

同时再在门极驱动上交叉禁止，使：

```text
USB_SW_EN AND 6V_SW_EN
```

在硬件上不可能同时成立。

最好再加一点 break-before-make dead time，例如几十到几百 µs；因为这些 P-MOS gate charge 约 25–35 nC @4.5 V，并非瞬间切换。AOS 给出了对应的 gate-charge 和 switching 参数。

这一部分只需要便宜的逻辑门/小信号晶体管，没必要上 power mux。

---

## 为什么不选 TPS25947

对 **USB 5 V / 3 A** 来说，它其实非常漂亮：

`TPS259470LRPWR / C3662793`

当前：

* 2.7–23 V
* 28 V abs max
* integrated back-to-back FET
* 28.3 mΩ
* true reverse-current blocking
* 0.5–6 A current limit
* short-circuit/thermal protection
* 当前嘉立创约 **$1.02/颗**

([LCSC Electronics][5])

但最大 continuous device rating 是 **5.5 A**，所以：

**不能用于你的 6 V / 6 A continuous path。**

TPS25948 才是合适的 8 A 集成版本：

* 3.5–23 V
* 28 V abs
* **8 A**
* **12.2 mΩ**
* back-to-back FET
* always reverse-current blocking
* short/thermal/OCP

从纯技术角度，它是 6 V path 的理想器件。([Texas Instruments][6])

但这条路线：

```text
TPS25948 + TPS25947
```

明显比：

```text
4 × inexpensive P-MOS + tiny gate logic
```

贵，而且你这里已经有外部互斥/安全链，不需要重复购买两套复杂 eFuse 功能。

---

# 最终选择

我建议把这一块确定为：

### Power FET

**4 × AONS21357-HXY / C22367089**

分配：

```text
Q6A1 + Q6A2 = 6V path back-to-back
QUSB1 + QUSB2 = USB path back-to-back
```

关键指标：

| 指标                        |                            结果 |
| ------------------------- | ----------------------------: |
| VDS                       |                          30 V |
| topology                  |        2× back-to-back / path |
| RDS(on) max @ −4.5 V      |                   12.3 mΩ/FET |
| 6 V / 6 A continuous      |                      **PASS** |
| 6 V / 8 A / 1 s           |                      **PASS** |
| USB / 3 A                 |                      **PASS** |
| 40°C thermal              |                      **PASS** |
| OFF leakage <100 µA       |                 **PASS，余量很大** |
| default OFF               | **通过 gate-source pull-up 实现** |
| failed-source isolation   |                      **PASS** |
| MCU-independent isolation |                       **可实现** |

四颗 MOSFET当前约：

**4 × $0.35 ≈ $1.40 ≈ ¥10。** ([LCSC Electronics][1])

再增加：

* 2× 小信号 NPN/NMOS gate pull-down；
* 2× 100 kΩ gate-source pull-up；
* 2× gate resistor；
* 低成本 one-hot/interlock logic；

预计整个 **6V/5V isolation switch** 可以控制在约 **¥11–14 BOM**。

这是我目前更倾向冻结的方案。相比用 TPS25948/TPS25947，它保留了你真正需要的**物理双向隔离**，但把保护功能拆给已有安全链处理，从而显著降低成本。

[1]: https://www.lcsc.com/zh-CN/product-detail/mosfets_hxy-mosfet-aons21357-hxy_C22367089.html "AONS21357-HXY | Single FETs, MOSFETs | HXY MOSFET | LCSC"
[2]: https://www.aosmd.com/products/mosfets/p-channel-mosfets-8v-60v/aons21357?utm_source=chatgpt.com "AONS21357 | AOSMD"
[3]: https://www.aosmd.com/sites/default/files/res/datasheets/AONS21357.pdf?utm_source=chatgpt.com "AONS21357
30V P-Channel MOSFET
General Description"
[4]: https://www.ti.com/lit/an/slva730/slva730.pdf?ref_url=https%253A%252F%252Fwww.ti.com%252Fproduct%252Fko-kr%252FTPS22953&ts=1781803479181&utm_source=chatgpt.com "Reverse Current Protection in Load Switches"
[5]: https://www.lcsc.com/product-detail/C3662793.html?utm_source=chatgpt.com "TPS259470LRPWR | Current Regulation/Management | TI | LCSC"
[6]: https://www.ti.com/product/TPS25948?utm_source=chatgpt.com "TPS25948 data sheet, product information and support | TI.com"


### C2 — servo current-sense amplifier

我建议直接选 **TI INA180A2IDBVR / 嘉立创 C192764**。它基本是你这个位置的“低成本标准答案”。

[嘉立创/LCSC：INA180A2IDBVR / C192764](https://www.lcsc.com/product-detail/C192764.html)
[TI 原厂 Datasheet](https://www.ti.com/lit/ds/symlink/ina180.pdf)

它的关键参数正好覆盖你的要求：

| 项目                |         INA180A2IDBVR |
| ----------------- | --------------------: |
| Supply            |         **2.7–5.5 V** |
| 3.3 V supply      |              **PASS** |
| Common-mode       |         **−0.2–26 V** |
| 0–7 V high-side   |         **PASS，大量余量** |
| Gain              |            **50 V/V** |
| Vos max           | **±150 µV @ VCM=0 V** |
| Vos max @ 12 V CM |           **±500 µV** |
| Offset drift      |       **1 µV/°C max** |
| Gain error        |           **±1% max** |
| Gain drift        |     **20 ppm/°C max** |
| Bandwidth（A2）     |           **210 kHz** |
| Output current    |            约 **8 mA** |
| Temp              |            −40–125 °C |
| Package           |          **SOT-23-5** |

TI 明确给出了 **offset 和 temperature drift 的 guaranteed maximum**，而不是只给 typical，这一点完全符合你的要求。([Texas Instruments][1])

### 成本

当前嘉立创库存接近 **10 万颗**：

* 5+：**$0.2239 ≈ ¥1.6/颗**
* 50+：$0.1782
* 150+：$0.1560
* 500+：$0.1361

所以在你的成本原则下，我认为没有必要为这个位置继续追更昂贵的 INA190/INA240 一类器件。([LCSC Electronics][2])

### 与 ESP32 ADC 的连接

INA180 的输出可以直接驱动一个普通 ADC RC 滤波器。推荐：

```text
SERVO_6V
   |
 RSHUNT
   |
SERVO_BUS
   |
   +---- INA180 IN+
   +---- INA180 IN-

INA180 OUT ---- 100 Ω ---- ESP32_ADC
                         |
                        10 nF
                         |
                        GND
```

即：

**100 Ω + 10 nF**

截止频率约：

$$
f_c = \frac{1}{2\pi RC}
\approx 159\,kHz
$$

如果你主要看的是舵机平均/峰值电流而不是高频 PWM 细节，我反而建议用更强一些的：

**1 kΩ + 100 nF → fc ≈ 1.59 kHz**

这样能明显降低舵机换相/PWM 对 ESP32 ADC 的干扰，同时 INA180 的 210 kHz 带宽完全足够。INA180 输出驱动能力也足以驱动这种 RC。([LCSC Electronics][3])

### 需要顺手确定 shunt

因为增益是 50 V/V：

$$
V_{OUT}=I\cdot R_{SHUNT}\cdot50
$$

如果你的 servo rail 需要测到 **8 A transient**，为了不超过 ESP32 约 3.3 V ADC：

若取：

**RSHUNT = 5 mΩ**

则：

* 1 A → 0.25 V
* 3 A → 0.75 V
* 6 A → 1.50 V
* 8 A → **2.00 V**

非常舒服。

同时 shunt 损耗：

* 6 A：\(36×0.005=0.18 W\)
* 8 A：\(64×0.005=0.32 W\)

因此可以用一颗 **5 mΩ、1 W、≤1%、低 TCR、Kelvin/4-terminal 优先**的电流采样电阻。

反过来，如果用 10 mΩ：

8 A × 10 mΩ × 50 = **4 V**

会超出 3.3 V ADC，所以不建议。

### 误差量级

用 5 mΩ shunt 时，INA180 的输入 offset 对等效电流的影响：

$$
I_{offset}=\frac{V_{OS}}{R_{SHUNT}}
$$

以 ±150 µV 计算：

$$
150\mu V / 5m\Omega = 30mA
$$

非常合理。

即使按较保守的 ±500 µV：

$$
500\mu V/5m\Omega = 100mA
$$

对于 0–8 A servo 总线监测仍然完全可接受。TI 给出的最大 offset、增益误差和 drift 都是有保证的。([Texas Instruments][4])

因此我建议冻结为：

**Servo current-sense amplifier = INA180A2IDBVR / C192764**

配套建议：

```text
VCC       = 3V3
GAIN      = 50 V/V
RSHUNT    = 5 mΩ / ≥1 W / ≤1% / low-TCR
ADC RC    = 1 kΩ + 100 nF
VCM range = servo 0–7 V
```

以现在约 **¥1.6/颗**的价格，这个位置继续降成本的收益已经很小，而换成无明确 guaranteed-max offset/drift 的国产 current-sense amplifier，反而会牺牲你明确要求的可验证性。([LCSC Electronics][3])

[1]: https://www.ti.com/product/INA180?utm_source=chatgpt.com "INA180 data sheet, product information and support | TI.com"
[2]: https://www.lcsc.com/product-detail/C192764.html?utm_source=chatgpt.com "INA180A2IDBVR | Instrumentation, Op Amps, Buffer Amps | TI | LCSC"
[3]: https://www.lcsc.com/zh-CN/product-detail/Current-Sense-Amplifiers_Texas-Instruments-INA180A2IDBVR_C192764.html?utm_source=chatgpt.com "INA180A2IDBVR | Instrumentation, Op Amps, Buffer Amps | TI | LCSC"
[4]: https://www.ti.com/lit/ds/symlink/ina180.pdf?utm_source=chatgpt.com "INAx180 Low- and High-Side Voltage Output, Current-Sense Amplifiers datasheet (Rev. H)"


`INA181A2` 可作为搜索起点，请给 exact suffix/C-code/datasheet/价格。

### C3 — 6 V rail TVS/clamp

我会把这一位冻结为 **6.5 V、单向、SMB 600 W TVS**，首选：

**Jingdao SMBJ6.5A / C353346**

而不是 6.0 V TVS，也不建议双向 TVS。它是目前“正常 6 V rail 不误动作 + 对 regen 快沿提供钳位 + 成本极低”之间最合理的点。

| 参数        |              SMBJ6.5A / C353346 |
| --------- | ------------------------------: |
| VRWM      |                       **6.5 V** |
| VBR       |                 **7.22–7.98 V** |
| VC max    | **11.2 V @ 53.6 A, 10/1000 µs** |
| IPP       |                      **53.6 A** |
| PPP       |          **600 W @ 10/1000 µs** |
| Polarity  |              **Unidirectional** |
| Package   |                  SMB / DO-214AA |
| LCSC 库存   |                           约 660 |
| 20+ price |             **$0.0357 ≈ ¥0.25** |

嘉立创资料直接列出了 6.5 V VRWM、11.2 V clamp、53.6 A / 600 W。([LCSC Electronics][1])

[嘉立创 SMBJ6.5A / C353346](https://www.lcsc.com/product-detail/C353346.html?utm_source=chatgpt.com)

### 为什么最低应该是 6.5 V，而不是 SMBJ6.0A

SY8388ARHC 是可调输出、±1% reference 的 8 A buck；Silergy 给出的工作输入为 4–24 V，输出可调，8 A continuous。

如果 nominal rail 是：

$$
V_{OUT}=6.00V
$$

只算 ±1% reference，正常高侧已经可能达到：

$$
6.00\times1.01=6.06V
$$

还没计反馈电阻 tolerance、动态 overshoot。

因此：

**VRWM = 6.0 V 的 SMBJ6.0A 不合格。**

它会让正常最大工作点进入 TVS 规定的 stand-off 边界之外；而且 6 V TVS 本身的 leakage 已可高达数百 µA。嘉立创例如 SMBJ6.0A 给出的 VRWM 就是 6.0 V、VC=10.3 V。([LCSC Electronics][2])

6.5 V 是这里比较合理的最低档：

```text
正常：
5 V USB        -> 不动作
6.0 V nominal  -> 不动作
~6.1 V max     -> 不动作

异常：
>~7.2 V        -> 开始 avalanche
regen spike    -> 强钳位
```

我暂时**不建议升到 6.8 V / 7 V**，因为没有收益：它只会进一步抬高开始钳位的电压。6.5 V 已经给正常 6 V rail 留出了约 400 mV 以上的静态余量。

### 但不要把“VC=11.2 V”误解成 rail 平时会升到 11.2 V

`11.2 V` 是 datasheet 在极端：

**53.6 A、10/1000 µs**

测试电流下规定的 maximum clamping voltage。([LCSC Electronics][3])

你的 servo regen 瞬态如果只有例如数安培，TVS 实际工作电压会明显低于 11.2 V。

从 datasheet 端点只能粗略观察它的动态斜率：

$$
r_d\approx
\frac{11.2-7.98}{53.6}
\approx60m\Omega
$$

因此在几安培量级，TVS 工作点大致会位于 **8 V 附近而不是 11.2 V**。

不过这一数字**不能当 guaranteed clamp specification**。原厂只保证表格中的 VBR 和 `VC @ IPP`。

这也是为什么你后面那个 **regen transient PVR 测试必须保留**。

---

# TVS 在这里的职责必须限定

这里非常重要：

**SMBJ6.5A 不应该承担舵机持续 regenerative braking 的能量。**

它的数据表 600 W 是：

> **10/1000 µs 单脉冲等级**

而同类 SMBJ 的重复脉冲额定通常只在很低 duty cycle 下成立，例如 LCSC 所列系列明确给出约 **0.01% repetition duty cycle**。([LCSC Electronics][3])

所以系统应该分成：

```text
                    fast spike
                       ↓
SERVO_6V ──────┬──── SMBJ6.5A ─── GND
               │
               ├──── bulk capacitor
               │
               └──── regen-management path
                         ↑
                    后续 PVR 验证
```

职责是：

**TVS**
→ ns–µs 快沿、换相尖峰、异常 overshoot 的最后一道硬钳位。

**Output polymer/capacitors**
→ 吸收短时间 regen charge。

**regen management / source architecture**
→ 处理 ms–s 级反电动势能量。

也就是说，后续 PVR 如果发现：

> servo repeatedly pushes rail above ~7.2–8 V for milliseconds

那不是把 SMBJ 换成 SMCJ1500W 就结束，而是应该增加**regen energy sink / active clamp / braking resistor / source absorption path**。

---

# 为什么选单向，而不是双向

这里我明确选择：

### **Unidirectional**

连接：

```text
SERVO_6V ─────── Cathode
                    |
                 SMBJ6.5A
                    |
GND ──────────── Anode
```

原因有三个。

### 1. 正常 6 V operation

TVS 反向偏置：

```text
6V rail → cathode
GND     → anode
```

因为：

$$
6V < 6.5V_{RWM}
$$

基本不参与工作。

### 2. source OFF、USB 5 V 接管

这是你特别要求检查的情况。

当 SY8388 / 6 V source 已经通过前面讨论的 back-to-back isolation switch 切断，而 USB 路径把 servo rail 拉到：

**5 V**

TVS仍然只是：

$$
5V < 6.5V_{RWM}
$$

因此：

```text
USB 5V
  ↓
servo rail
  ↓
SMBJ6.5A：OFF
```

**不会把 USB rail 钳掉。**

所以它和你的 6 V / USB 5 V mutually-exclusive architecture兼容。

### 3. Negative transient

这是选择单向最有价值的地方。

如果舵机线束、电感或地弹导致：

```text
SERVO_RAIL < GND
```

单向 TVS 此时不是 avalanche，而是变成普通正向二极管：

```text
GND → TVS → SERVO_RAIL
```

因此负向 excursion 大致会被限制在：

**约 −0.7～−1.x V**

而不是允许 rail 掉到很多伏负电压。

对于：

* SY8388 output；
* servo electronics；
* current-sense amplifier；
* output capacitors；

这明显更合理。

---

## 双向 TVS 反而有问题

如果改成：

**SMBJ6.5CA**

那么负向 transient 要一直到接近：

$$
-7\sim-8V
$$

才开始 avalanche。

这意味着：

```text
servo rail:
0 → -1V → -3V → -5V …
```

在相当大的区间内 TVS **什么都不做**。

这对一个本质上永远应该是：

$$
0\ldots6V
$$

的单极性电源轨没有任何优势。

双向 TVS 更适合：

* AC；
* RS-485；
* CAN；
* 双极性信号；

不是这里。

因此：

> **A = yes**
>
> **CA = no**

---

# SMAJ6.5A vs SMBJ6.5A

我也比较了更小、更便宜的 SMA。

例如：

**Jingdao SMAJ6.5A / C169450**

参数：

* VRWM 6.5 V
* VBR 7.22–7.98 V
* VC 11.2 V
* IPP 35.7 A
* **400 W**
* SMA
* 56k+ 库存
* $0.0377/20+。([LCSC Electronics][4])

奇怪的是，目前它甚至**没有比 SMB 600 W 版本便宜**。

因此：

**没有理由为了省面积牺牲 1/3 pulse capability。**

选择 SMBJ。

---

# 我不会选来源参数异常的“SMBJ6.8A”

搜索时发现一些廉价国产 SMBJ6.8A 商品页实际上写：

**VRWM = 5.8 V**

例如 SURGING 和 UMW 的部分 6.8A 页面就是如此。([LCSC Electronics][5])

这说明不同厂家的低压 TVS 命名/表格存在很容易踩坑的地方。

所以本设计不要根据：

> “型号里写 6.8”

判断。

BOM freeze 必须记录：

> **VRWM from exact manufacturer datasheet**

而不是只记录 MPN。

---

# 关于“低于 SY8388 / servo connector 危险电压”

这里需要把两个东西分开。

**SY8388 本身并不是 7 V 绝对最大器件。** Silergy 原厂给出的 SY8388ARHC 输入工作范围本身就是最高 **24 V**；它是 8 A / 24 V buck。

真正容易被 regen overvoltage 杀死的是：

> **6 V servo electronics**

而不是因为 SY8388 是个“7 V 芯片”。

因此真正决定 TVS 是否足够的是：

**你的具体舵机允许的 absolute maximum servo supply。**

如果 servo electronics 的真正绝对最大值是：

* **≥10–12 V**：SMBJ6.5A 很舒服；
* **≈8.4 V**：SMBJ6.5A 可以作为 spike suppressor，但**不能声称 worst-case guaranteed ≤8.4 V**；
* **≤7.4 V**：任何标准 avalanche TVS 都很难同时满足 `VRWM > normal 6 V max` 和严格 `VC <7.4 V`，应采用 active clamp。

这个 distinction 很重要。

---

# 因此我建议冻结

### **TVS1 = SMBJ6.5A / C353346**

```text
Type       = unidirectional TVS
VRWM       = 6.5 V
VBR        = 7.22–7.98 V
VC(max)    = 11.2 V @ 53.6 A
PPP        = 600 W / 10/1000 µs
Package    = SMB / DO-214AA
Connection = cathode → SERVO_6V
             anode   → power GND
```

选择理由：

**✓** VRWM 高于正常 6 V rail 最大值
**✓** USB 5 V 模式不会误导通
**✓** positive regen overshoot 有强 avalanche clamp
**✓** negative transient 自动正向 clamp
**✓** 比双向 TVS 更适合单极性 servo rail
**✓** 600 W 比 SMA 400 W 更有 PVR 余量
**✓** 价格只有约 **¥0.25/颗**，没有进一步压成本的必要
**✓** 后续 regen PVR 仍作为必须验证项目，而不是假定 TVS 能无限重复吸收舵机回生能量。([LCSC Electronics][1])

目前我只会保留一个条件性问题：**在最终 PVR 之前，不能把“servo rail 永远 <8.x V”写成 verified requirement**，因为标准 SMBJ6.5A 的 datasheet 并没有在你实际 regen 电流下给 guaranteed clamp 曲线。后续 PVR 应实测 **rail peak、TVS peak current、pulse width、repeat rate 和 TVS temperature rise**，由这五项最终判断 SMBJ6.5A 是纯 spike clamp，还是还需要额外 active regen clamp。

[1]: https://www.lcsc.com/product-detail/C353346.html?utm_source=chatgpt.com "SMBJ6.5A | Jingdao Microelectronics | Price | In Stock | LCSC Electronics"
[2]: https://www.lcsc.com/product-detail/C713666.html?utm_source=chatgpt.com "SMBJ6.0A | LGE | Price | In Stock | LCSC Electronics"
[3]: https://www.lcsc.com/product-detail/C113978.html?utm_source=chatgpt.com "SMBJ6.5A | MDD(Microdiode Semiconductor) | Price | In Stock | LCSC Electronics"
[4]: https://www.lcsc.com/product-detail/C169450.html?utm_source=chatgpt.com "SMAJ6.5A | Jingdao Microelectronics | Price | In Stock | LCSC Electronics"
[5]: https://www.lcsc.com/product-detail/C2959784.html?utm_source=chatgpt.com "SMBJ6.8A | SURGING | Price | In Stock | LCSC Electronics"


## D. Safety logic and connectors

### D1 — AON fault-latch logic

可以。按你的“**成本优先，但关键逻辑必须有原厂 truth table/threshold**”原则，我建议 AON fault-latch logic 统一采用 **74LVC 小逻辑族**，其中 DFF 选中微爱芯，门电路优先也用同一家，减少供应商和逻辑族混用。

我建议冻结成下面这一组：

| 功能                        | 推荐 MPN                   |   LCSC C-code | 封装      | 3.3 V      |                  参考价格 |
| ------------------------- | ------------------------ | ------------: | ------- | ---------- | --------------------: |
| **DFF + async SET/RESET** | **AiP74LVC1G74TA8.TR**   | **C52121053** | TSSOP-8 | 1.65–5.5 V | **$0.1605/5+ ≈ ¥1.1** |
| **Schmitt inverter**      | **AiP74LVC1G14GC353.TR** |  **C3294716** | SOT-353 | 1.65–5.5 V |               ~$0.097 |
| **OR**                    | **AIP74LVC1G32GC353.TR** |  **C4945071** | SOT-353 | 1.65–5.5 V |               ~$0.099 |
| **AND（若需要）**              | **AiP74LVC1G08GC353.TR** |  **C3002790** | SOT-353 | 1.65–5.5 V |               ~$0.098 |
| 单独 inverter               | **不需要**                  |             — | —       | —          |                     — |

原因是 `1G14` 本身就是 **Schmitt inverter**，因此可以同时承担你要求的“Schmitt input gate”和某一级反相，不必再单独放 `1G04`。这比“1G17 + 1G04”省一颗器件。

### 1. DFF：AiP74LVC1G74TA8.TR / C52121053

这是我最明确推荐的器件。

它是：

* 单路 positive-edge DFF；
* 独立 `/SD` asynchronous SET；
* 独立 `/RD` asynchronous RESET；
* Q 和 /Q 都有；
* 1.65–5.5 V；
* −40～+125 °C 的 LCSC 参数；
* 输入允许到 5 V；
* `IOFF` 支持掉电状态下避免 back-power。

嘉立创目前约 2275 颗现货，5+ 为 **$0.1605**。相比 TI `SN74LVC1G74DCTR / C840104` 当前约 $0.75 以上，成本差距非常大。([LCSC Electronics][1])

[AiP74LVC1G74TA8.TR / C52121053 嘉立创](https://www.lcsc.com/product-detail/C52121053.html?utm_source=chatgpt.com)
[中微爱芯 AiP74LVC1G74 Datasheet](https://uploadcdn.oneyac.com/attachments/files/brand_pdf/i-core/2C/03/AiP74LVC1G74-DATASHEET.pdf?utm_source=chatgpt.com)

原厂 asynchronous truth table 是：

| /SD | /RD | CP | D |     Q | /Q |
| --: | --: | -- | - | ----: | -: |
|   0 |   1 | X  | X | **1** |  0 |
|   1 |   0 | X  | X | **0** |  1 |
|   0 |   0 | X  | X |     1 |  1 |
|   1 |   1 | ↑  | 0 |     0 |  1 |
|   1 |   1 | ↑  | 1 |     1 |  0 |

所以 fault latch 最适合把：

* `/RD` 用作 **power-up/default clear**；
* `/SD` 或 clock/data 路径承担 fault latch set，

具体取决于你最终 latch 极性。

原厂 datasheet 明确说明 SET 和 RESET 都是 **active-low asynchronous**，并且独立于 clock。([Oneyac][2])

需要特别注意：

**`/SD = 0` 且 `/RD = 0` 是非法/不推荐状态。**

此时 Q=/Q 都为 1。因此原理图必须保证两个异步输入不会同时长期拉低。([Oneyac][2])

### 2. 3.3 V threshold 有明确保证

AiP74LVC1G74 在：

`VCC = 2.7–3.6 V`

下规定：

* **VIH(min) = 2.0 V**
* **VIL(max) = 0.8 V**

而不是只给 typical。([ElecFans PDF][3])

所以 CONTROL_AON_3V3 下：

```text
LOW guaranteed  <= 0.8 V
HIGH guaranteed >= 2.0 V
```

非常适合 ESP32 / comparator / supervisor 的 3.3 V logic。

---

## Power-up 行为：不能依赖 DFF 自己随机上电

这里有一个很重要的设计结论：

**74LVC1G74 本身并不保证没有 async control 时 Q 的上电状态。**

TI 专门有一份《Power-Up Behavior of Clocked Devices》，明确说明普通 flip-flop / clocked logic 在第一个有效 clock/reset/set 之前，power-up output 可能是不确定的；正确方法就是利用 asynchronous preset/clear，并用外部 pull resistor 在上电时强制其中一个状态。([Texas Instruments][4])

因此你的要求：

> 输入掉电默认可由外部电阻确定

完全正确，而且应该在原理图里**显式实现**。

我建议默认安全态：

```text
/RD ---- 47k~100k ---- GND
```

也就是：

> CONTROL_AON_3V3 刚起来、其他逻辑还没起来时
> `/RD = 0`
> → Q = 0
> → FAULT_LATCH / SERVO_ALLOW 默认进入安全状态

然后由 AON supervisor / POR 信号主动把 `/RD` 拉高，才释放 latch。

不要写成：

```text
/RD ---- pull-up ---- 3V3
```

然后期待 flip-flop 自己 power-up 为 0。

那是不受保证的。([Texas Instruments][5])

---

# 3. Schmitt input：AiP74LVC1G14GC353.TR / C3294716

这里我选 **Schmitt inverter**，而不是 buffer。

原因很简单：

你大概率本来就需要某一级 inversion，所以：

> `1G14 = Schmitt conditioning + inverter`

一次完成两个功能。

嘉立创：

**AiP74LVC1G14GC353.TR / C3294716 / SOT-353**

当前约 $0.097。([LCSC Electronics][6])

[AiP74LVC1G14GC353.TR / C3294716](https://www.lcsc.com/product-detail/C3294716.html?utm_source=chatgpt.com)

如果逻辑极性最终必须保持同相，再换成：

**AIP74LVC1G17GC353.TR / C4945070**

即可。它是单路 Schmitt buffer。([LCSC Electronics][7])

[AIP74LVC1G17GC353.TR / C4945070](https://www.lcsc.com/product-detail/C4945070.html?utm_source=chatgpt.com)

### Schmitt threshold

这颗系列的 datasheet 很好的一点，是不只写“Schmitt Trigger”，而是真的给出了 threshold range。

以 3.0 V VCC 为例：

**−40～+105 °C guaranteed：**

* `VT+ = 1.26 … 1.71 V`
* `VT− = 0.86 … 1.37 V`
* hysteresis `VH >= 0.21 V`

典型约：

* VT+ ≈ 1.5 V
* VT− ≈ 1.1 V

因此 slow fault signal、RC delay、open-drain supervisor 输出都可以放心接，不必担心普通 CMOS 门在慢边沿附近抖动。([ElecFans PDF][8])

---

# 4. OR：AIP74LVC1G32GC353.TR / C4945071

推荐：

**AIP74LVC1G32GC353.TR**

* SOT-353
* 1.65–5.5 V
* 24 mA class output
* IOFF partial-power-down protection
* −40～+105 °C
* 2-input positive OR。

原厂 truth table：

|  A |  B |  Y |
| -: | -: | -: |
|  0 |  0 |  0 |
|  0 |  1 |  1 |
|  1 |  0 |  1 |
|  1 |  1 |  1 |

原厂资料也明确给出 1.65–5.5 V 供电和 partial-power-down `IOFF`。([LCSC 数据表][9])

对应 C-code：

**C4945071**。

这一颗大约 $0.10 量级。

---

# 5. AND：AiP74LVC1G08GC353.TR / C3002790

如果最终安全链需要：

```text
SERVO_ALLOW =
    SYSTEM_OK
  & !FAULT_LATCH
```

或者类似 one-hot enable，那么推荐：

**AiP74LVC1G08GC353.TR / C3002790**

SOT-353，1.65–5.5 V。嘉立创约 **$0.0982/10+**，库存约 2200。([LCSC Electronics][10])

[AiP74LVC1G08GC353.TR / C3002790](https://www.lcsc.com/product-detail/C3002790.html?utm_source=chatgpt.com)

而且有一个额外好处：

**AiP74LVC1G08 的输入本身也是 Schmitt-trigger。**

嘉立创和原厂描述都明确写了 Schmitt action。([LCSC Electronics][10])

所以如果你的 fault signal 恰好直接进入 AND gate，甚至可能把独立 `1G14/1G17` 再省掉。

这要看最终 boolean equation。

---

# 我更推荐怎样组合

如果原理图逻辑允许，我会尽量压成：

```text
1 × AiP74LVC1G74
1 × AiP74LVC1G14
1 × AiP74LVC1G32
```

需要 AND 时才增加：

```text
1 × AiP74LVC1G08
```

不要先默认放：

```text
Schmitt buffer
+ inverter
+ OR
+ AND
```

因为 LVC 小逻辑中很多门本身已经是 Schmitt input，可以合并功能。

以当前价格估计：

```text
1G74   ¥1.1
1G14   ¥0.7
1G32   ¥0.7
----------------
核心 latch logic ≈ ¥2.5
```

即便再加一个 `1G08`，核心逻辑 IC 也大约 **¥3.2 左右**。

远低于全部采用 TI little-logic。

---

## 对掉电输入的处理

所有外部 asynchronous / enable 输入都不要浮空。

建议：

```text
active-high unsafe signal:
INPUT ---- 47k~100k ---- GND

active-low safety signal:
/INPUT ---- 47k~100k ---- GND or 3V3
```

到底上拉还是下拉，应由**失电时哪个状态更安全**决定。

而不是由逻辑习惯决定。

对 fault latch 我倾向定义成：

```text
Q = 1  => FAULT_LATCHED
Q = 0  => NO_FAULT
```

这样 power-up 用 `/RD` 强制：

```text
Q = 0
```

然后真正 fault 通过 `/SD` 或 D/CLK：

```text
Q -> 1
```

而 servo enable 再使用：

```text
SERVO_ENABLE =
SYSTEM_READY
AND
NOT FAULT_LATCHED
```

这样整个逻辑含义最清晰。

---

# 最终建议 BOM

我会先冻结为：

```text
U_AON_FF
AiP74LVC1G74TA8.TR
C52121053
TSSOP-8
```

```text
U_AON_SCHMITT_INV
AiP74LVC1G14GC353.TR
C3294716
SOT-353
```

```text
U_AON_OR
AIP74LVC1G32GC353.TR
C4945071
SOT-353
```

可选：

```text
U_AON_AND
AiP74LVC1G08GC353.TR
C3002790
SOT-353
```

并明确设计规则：

**DFF power-up state 不得依赖器件随机上电；必须通过 `/RD` 或 `/SD` 外部偏置得到确定的 safe state。** TI 的 power-up application note也明确支持这种做法。([Texas Instruments][5])

如果你接下来把 **AON fault-latch 的具体 boolean equation / fault inputs / reset方式**给我，我下一步可以直接把这四颗进一步削减，判断到底能不能做到 **1G74 + 2 个 single-gate**，甚至只剩 **3 颗 IC**。

[1]: https://www.lcsc.com/product-detail/C52121053.html?utm_source=chatgpt.com "AiP74LVC1G74TA8.TR | Wuxi I-core Elec | Price | In Stock | LCSC Electronics"
[2]: https://uploadcdn.oneyac.com/attachments/files/brand_pdf/i-core/2C/03/AiP74LVC1G74-DATASHEET.pdf?utm_source=chatgpt.com "Wuxi I-CORE Electronics Co., Ltd."
[3]: https://pdf.elecfans.com/p/11195553.html?utm_source=chatgpt.com "AiP74LVC1G74YA8.TR,AiP74LVC1G74YA8.TR pdf,AiP74LVC1G74YA8.TR中文资料,AiP74LVC1G74YA8.TR引脚图,AiP74LVC1G74YA8.TR电路_datasheet网"
[4]: https://www.ti.com/lit/an/scha005b/scha005b.pdf?ts=1781405632018&utm_source=chatgpt.com "Power-Up Behavior of Clocked Devices (Rev. B)"
[5]: https://edgeworker.ti.com/lit/an/scha005b/scha005b.pdf?ref_url=https%253A%252F%252Fedgeworker.ti.com%252Fproduct%252FSN74AUC1G79&ts=1779711798155&utm_source=chatgpt.com "Power-Up Behavior of Clocked Devices (Rev. B)"
[6]: https://www.lcsc.com/product-detail/C3294716.html?utm_source=chatgpt.com "AiP74LVC1G14GC353.TR | Wuxi I-core Elec | Price | In Stock | LCSC Electronics"
[7]: https://www.lcsc.com/product-detail/C4945070.html?utm_source=chatgpt.com "AIP74LVC1G17GC353.TR | Wuxi I-core Elec | Price | In Stock | LCSC Electronics"
[8]: https://pdf.elecfans.com/p/11077583.html?utm_source=chatgpt.com "AIP74LVC1G17GC353.TR,AIP74LVC1G17GC353.TR pdf,AIP74LVC1G17GC353.TR中文资料,AIP74LVC1G17GC353.TR引脚图,AIP74LVC1G17GC353.TR电路_datasheet网"
[9]: https://datasheet.lcsc.com/lcsc/2305291037_Wuxi-I-core-Elec-AIP74LVC1G32GC353-TR_C6798237.pdf?utm_source=chatgpt.com "Wuxi I-CORE Electronics Co., Ltd."
[10]: https://www.lcsc.com/product-detail/C3002790.html?utm_source=chatgpt.com "AiP74LVC1G08GC353.TR | Wuxi I-core Elec | Price | In Stock | LCSC Electronics"

### D2 — ESTOP connector/ESD

- 2P locking PCB connector，低电流信号用途；
- 不接受直接焊线；
- exact mating housing/terminal系列明确；
- 外部线缆ESD保护可与H5VL10B共料，但需确认AON 3.3 V和漏电。

### D3 — 5×20 fuse and holder

- 10 A fast ceramic sand-filled fuse；≥32 VDC；DC breaking目标≥250 A；
- PCB holder exact MPN；额定电流≥10 A；
- contact resistance、温升、保持力和footprint资料；
- fuse/holder必须分别给C-code和manufacturer PDF。

## E. Existing supervisor confirmation

请确认 `TPS3808G33DBVR-TP / C51886189` 的原厂/品牌身份及可搜索文字版datasheet。现有本地PDF是图像型资料，仍需确认：

- 3.3 V sense threshold全温min/max；
- CT delay equation/range；
- MR minimum pulse；
- RESET output type和sink/leakage；
- exact SOT-23-6 pinout。
