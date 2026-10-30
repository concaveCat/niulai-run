# 牛来快跑 Hardware Parameter Register v0.1

状态：**参数收集阶段；允许编写参数化底层驱动，未授权实体驱动接入/通电**
日期：2026-09-10
Owner：Project maintainer

本文是外围软件和实体驱动的集中参数登记表。所有尚未定义、尚未实测或存在来源冲突的参数必须先记录在这里。Operator 完成接线/通电/示波器/万用表/装配测试后，可直接在本文件补填数值、日期、仪器和证据位置。

## 0. 驱动开发闸门

在 Operator 明确确认本文件、并完成下列阻塞项前，不把候选参数接入实体自动运行或进行实体通电测试；允许先编写不依赖实物冻结值的参数化 native component 和 mock：

Operator 已明确授权先实现参数化底层驱动。当前实现允许寄存器/总线协议、通用初始化候选和 mock/native 编译测试；所有未实测外围参数仍为 `TBD` 或 `GENERIC_DEFAULT`，舵机缺少标定时角度命令必须拒绝执行，且不得进行实体运动测试。

- [ ] 本文件中的 Gerber 实际 GPIO 映射已由 Operator 确认；
- [ ] PCA9685 的 `VCC`、`OE` 电平、I²C 地址和上电行为已确认；
- [ ] ESP32 实物型号、排针方向和供电方式已确认；
- [ ] TFT、MAX98357A、MS4030、MPU6500 的实物针脚/电压已确认；
- [ ] 四个 KW10 的 `FOOT1..4` 与 FL/FR/RL/RR 的安装对应关系已确认；
- [ ] KW10 的通断逻辑和去抖要求已完成一次装配测试；
- [ ] 舵机通道、关节零位、脉宽范围和机械限位已记录；
- [x] Operator 已批准开始参数化底层驱动实现；实体接入/通电仍需另行按安全条件验收。

本文件的填写不等于驱动开发授权。

## 1. 证据优先级

1. Operator 指定的 PCA9685 说明书：`archive/客户参考9685-16路PWM/adafruit-16-channel-pwm-slash-servo-shield.pdf`；PCA9685 模块丝印和 Operator 的实物确认优先于说明书中针对 Adafruit Shield 的 VCC 接法描述；
2. 已制造 Gerber 及其中的 `FlyingProbeTesting.json`：物理 Carrier 网络/GPIO 以此为准；
3. Operator 的实物接线、万用表、示波器和台架记录；
4. 当前原理图和接口表：只作为候选输入，不能覆盖 Gerber 或实测；
5. 商品图推断：只能作为候选证据，不能作为已确认事实。

## 2. PCA9685 模块——按说明书登记

| 参数 | 当前登记 | 状态/后续填写 |
| --- | --- | --- |
| 芯片/功能 | 16 路、12-bit PWM/Servo Driver，经 I²C 控制 | MANUAL_CONFIRMED |
| I²C 默认地址 | `0x40`；地址跳线给基地址增加偏移 | MANUAL_CONFIRMED；实物跳线仍需记录 |
| PWM 通道 | CH0–CH15；当前 Carrier 只计划使用 8 路 | MANUAL_CONFIRMED / SYSTEM_TBD |
| PWM 计数 | 0–4095 | MANUAL_CONFIRMED |
| 说明书示例频率 | 60 Hz | EXAMPLE_ONLY；不得视为 MG90S 最终标定 |
| 软件通用默认频率 | 50 Hz | GENERIC_DEFAULT；可由配置覆盖，不能视为实测 |
| 说明书 VCC | Arduino 侧 5 V 逻辑电源 | MANUAL_REFERENCE；该说明书针对 Shield，不覆盖模块自身 3–5 V 丝印 |
| 说明书 V+ | 舵机电源 5–6 V，独立于 VCC | MANUAL_CONFIRMED |
| PCA9685 模块 VCC 范围 | 模块丝印 `3–5 V`，Operator 已确认 | OPERATOR_CONFIRMED |
| 当前 Carrier VCC | `3V3` | ACCEPTED_WITH_MODULE_MARKING |
| 当前 OE 上拉 | Carrier 上 `OE → 3V3`，10 kΩ | GERBER_CONFIRMED；与当前 3V3 VCC 配置一致 |
| OE 语义 | 低有效；HIGH 禁止输出/高阻，LOW 允许输出 | MANUAL/CHIP_CONFIRMED |
| 舵机通道映射 | `CH0..CH7 → SERVO_1..SERVO_8` | CURRENT_CANDIDATE；关节映射待 Operator 填写 |
| 每路频率/脉宽 | 未冻结 | 待填：频率、最小/中位/最大脉宽、对应 tick |
| 读回/错误处理 | 未定义 | 待填：I²C ACK、初始化失败、总线卡死、重试策略 |

说明书随附库提供的协议参考是：默认地址 `0x40`；通道起始寄存器为 `0x06 + 4×channel`；每路写入 `ON_L/ON_H/OFF_L/OFF_H` 四个字节；频率通过 `PRESCALE` 设置。参数化实现位于 [`firmware/platform/pca9685.py`](../../firmware/platform/pca9685.py)，通用默认值集中在 [`firmware/platform/defaults.py`](../../firmware/platform/defaults.py)，仅接受注入的 I²C 写总线，不代表 ESP-IDF 硬件端口或实体验证已经完成。

### PCA9685 待实测/待决定记录

| 项目 | 结果 | 日期/仪器/证据 |
| --- | --- | --- |
| 模块 VCC 实测值 | `__________ V` | `________________` |
| 模块 V+ 实测值 | `__________ V` | `________________` |
| I²C 扫描地址 | `__________` | `________________` |
| A0–A5/地址焊桥状态 | `__________` | `________________` |
| OE 上电电压 | `__________ V` | `________________` |
| OE 被 GPIO 拉低电压 | `__________ V` | `________________` |
| CH0 输出禁用时波形 | `__________` | `________________` |
| 模块 V+ 路径温升/压降 | `__________` | `________________` |

## 3. Gerber 实际 GPIO / 网络登记

以下来自 `Gerber_PCB1_2026-09-02.zip` 的 `FlyingProbeTesting.json`，并结合 ESP32 两排针从天线端到 USB 端的物理位置解释。Gerber 审阅记录明确要求 GPIO4/5 以实际 Gerber 为准。[Gerber 复核](PCB_GERBER_REVIEW_2026-09-02.md)

| Gerber 网络 | ESP32 物理位置 | GPIO | 当前用途 | 状态 |
| --- | --- | ---: | --- | --- |
| `FOOT1` | 右排第 6 个焊盘 | GPIO42 | KW10 足端输入 / RL | GERBER_NET_CONFIRMED；腿位为 Operator 当前映射 |
| `FOOT2` | 右排第 7 个焊盘 | GPIO41 | KW10 足端输入 / RR | GERBER_NET_CONFIRMED；腿位为 Operator 当前映射 |
| `FOOT3` | 右排第 8 个焊盘 | GPIO40 | KW10 足端输入 / FL | GERBER_NET_CONFIRMED；腿位为 Operator 当前映射 |
| `FOOT4` | 右排第 9 个焊盘 | GPIO39 | KW10 足端输入 / FR | GERBER_NET_CONFIRMED；腿位为 Operator 当前映射 |
| `AMPDIN` | 左排第 4 个焊盘 | GPIO4 | MAX98357A DIN | GERBER_CONFIRMED |
| `ST7789BL` | 左排第 5 个焊盘 | GPIO5 | TFT 背光控制 | GERBER_CONFIRMED |
| `ST7789CS` | 左排第 6 个焊盘 | GPIO6 | TFT CS | GERBER_CONFIRMED |
| `ST7789DC` | 左排第 7 个焊盘 | GPIO7 | TFT D/C | GERBER_CONFIRMED |
| `ST7789RST` | 左排第 9 个焊盘 | GPIO16 | TFT RESET | GERBER_CONFIRMED |
| `ST7789MOSI` | 左排第 10 个焊盘 | GPIO17 | TFT MOSI/SDA | GERBER_CONFIRMED |
| `ST7789SCLK` | 左排第 12 个焊盘 | GPIO8 | TFT SCLK | GERBER_CONFIRMED |
| `I2SBCLK` | 左排第 15 个焊盘 | GPIO9 | MAX98357A BCLK / MS4030 SCK | GERBER_CONFIRMED |
| `I2SWS` | 左排第 16 个焊盘 | GPIO10 | MAX98357A LRC/WS / MS4030 WS | GERBER_CONFIRMED |
| `I2CSDA` | 左排第 17 个焊盘 | GPIO11 | PCA9685 SDA | GERBER_CONFIRMED |
| `I2CSCL` | 左排第 18 个焊盘 | GPIO12 | PCA9685 SCL | GERBER_CONFIRMED |
| `MPUINT` | 右排第 18 个焊盘 | GPIO21 | MPU6500 INT | GERBER_CONFIRMED |
| `MICSD` | 左排第 20 个焊盘 | GPIO14 | MS4030 SD | GERBER_CONFIRMED |
| `OE` | 右排第 4 个焊盘 | GPIO1 | PCA9685 OE | GERBER_CONFIRMED；需与 Carrier 上拉实物核对 |

### Gerber 与旧候选接口表的冲突

旧的 `MODULAR_CARRIER_INTERFACE_TABLE_v0.2.csv` 仍写有 `GPIO8/9 = PCA I²C`、`GPIO5/6/7/15 = FOOT` 等候选映射。已制造 Gerber 给出的实际网络不同；当前采用 `FOOT1..4 = GPIO42/41/40/39`，并按 Operator 指定映射为 `GPIO39=FR、GPIO40=FL、GPIO41=RR、GPIO42=RL`。C-NC 的有效触发电平和机械安装后的触地语义仍需实物确认。

## 4. KW10 足端开关

| 参数 | 当前登记 |
| --- | --- |
| 器件 | KW10 |
| 网络 | `FOOT1..FOOT4` |
| GPIO | `FOOT1..4 = GPIO42/41/40/39`；Gerber 网络位于右排第 6–9 个焊盘 |
| FL/FR/RL/RR 对应 | `FL=GPIO40、FR=GPIO39、RL=GPIO42、RR=GPIO41`；Operator 当前软件映射，实体安装待台架确认 |
| always-on / always-off 语义 | 未确认，必须通过实际装配验证 |
| 上拉/下拉 | 候选原理图为 10 kΩ 到 3V3；装配实物状态待确认 |
| 触点形式 | C-NC；Operator 已确认按钮可用，机械安装后的有效触发方向待确认 |
| 去抖 | 100 Hz 采样、2 tick（20 ms 候选）；实际抖动时长待实测 |
| 采样周期 | 未定义 |
| 触地判定 | 未定义：按下/释放哪一种代表 contact |

待填测试记录：

| 开关 | 未按/悬空电平 | 按下电平 | 机械实际状态 | 去抖观察 |
| --- | --- | --- | --- | --- |
| FOOT1 | `_____ V / _____` | `_____ V / _____` | `always ______` | `__________` |
| FOOT2 | `_____ V / _____` | `_____ V / _____` | `always ______` | `__________` |
| FOOT3 | `_____ V / _____` | `_____ V / _____` | `always ______` | `__________` |
| FOOT4 | `_____ V / _____` | `_____ V / _____` | `always ______` | `__________` |

## 5. ST7789 TFT

### 已知网络

| 参数 | 当前登记 |
| --- | --- |
| 分辨率 | `170 × 320`，Operator 已确认 |
| 预期接口 | 4-wire write-only SPI：SCLK、MOSI、CS、D/C；另有 RESET、BL |
| MISO | 不需要用于基本写屏，当前未布线 |
| 像素格式候选 | RGB565 / 16-bit，通过 `COLMOD (0x3A)` 设置为 `0x55` |
| 地址窗口命令 | `CASET (0x2A)`、`RASET/PASET (0x2B)`、`RAMWR (0x2C)` |
| 基础初始化候选 | `SWRESET (0x01)` → 等待；`SLPOUT (0x11)` → 等待；设置 `COLMOD`/`MADCTL`；`DISPON (0x29)` |
| 初始化状态 | 上述为 ST7789 通用候选，不是该屏幕的已验证命令表 |

| 信号 | GPIO |
| --- | ---: |
| SPI SCLK | GPIO8 |
| SPI MOSI | GPIO17 |
| CS | GPIO6 |
| D/C | GPIO7 |
| RESET | GPIO16 |
| BL/backlight | GPIO5 |
| MISO | 未布线/未定义 |

### 未定义参数

- 面板实际控制器是否确为 ST7789 及其变体；
- 170×320 面板的 X/Y offset、旋转方向和 `MADCTL` 值；
- `VCC`、I/O 电压、背光电压和背光最大电流；
- RESET、D/C、CS 的有效电平和上电时序；
- 完整初始化命令表、gamma、反转模式、色彩格式最终值；
- SPI 频率、DMA 能力和刷新区域策略；
- 背光是直接 GPIO 控制还是需要三极管/恒流驱动。

所需驱动：SPI 总线、ST7789 初始化/复位、地址窗口/区域刷新、颜色格式和背光控制。上述候选值可以作为后续驱动设计起点，但必须在 Operator 批准后、接屏验证前才可实现。

## 6. MAX98357A 音频功放

### 公开通用参数（候选，不覆盖实物确认）

按 Analog Devices 的 MAX98357A/MAX98357B 数据手册，MAX98357A 支持标准 I²S，MAX98357B 才是左对齐变体；I²S/左对齐支持 8–96 kHz、16/24/32-bit 数据，且不需要 MCLK。芯片单电源范围为 2.5–5.5 V；官方资料给出 5 V、4 Ω 条件下约 3.2 W 的典型能力。模块上的实际芯片丝印、SD_MODE/GAIN 电阻和散热能力仍未确认，因此这些只作为软件配置候选，不能写成实物事实。

本项目的初始候选可记录为：标准 I²S、无 MCLK、16-bit 或 32-bit slot、采样率 `TBD`；例如 48 kHz/32-bit stereo 会产生 3.072 MHz BCLK，但不冻结为最终值。MAX98357A 的声道选择、增益和 SD/EN 状态必须按模块实物确认。

来源：<https://www.analog.com/en/products/max98357a.html>；<https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf>

### 已知网络

| 信号 | GPIO/连接 |
| --- | --- |
| VCC | 当前候选为 3V3，实物电压范围待确认 |
| GND | GND |
| DIN | GPIO4 |
| BCLK | GPIO9 |
| LRC/WS | GPIO10 |
| SPK+ / SPK- | BTL 输出，均不得接 GND |
| 模块第 3/4 针 | 商品图候选为 NC，待实物确认 |

### 未定义参数

- 实际模块芯片丝印：项目目标为 `MAX98357A`，但模块实物仍需确认不是 MAX98357B 或兼容板；
- VCC 范围和峰值电流；
- SD/EN 的默认状态与是否需要控制；
- GAIN 配置；
- I²S 采样率、位宽、slot 宽度、左右声道/数据对齐方式；通用候选为 MAX98357A 标准 I²S，不替代实物确认；
- 喇叭阻抗、额定功率、线缆和热条件；
- 上电静音、关机静音和故障处理。

所需驱动：ESP32 I²S TX、音频缓冲/DMA、采样率和格式配置、功放启停/静音（若实物提供该控制）。

## 7. MS4030 麦克风

### 公开通用参数（候选，不覆盖实物确认）

公开的 MS4030 模块手册把它描述为兼容 INMP441 的 I²S 数字 MEMS 麦克风，并给出 24-bit 数据、`VDD=1.6–3.6 V`、约 `1.4 mA`、约 `61 dBA SNR`、约 `-26 dBFS` 灵敏度和约 `60 Hz–15 kHz` 频响；`L/R=LOW` 选择左声道，`L/R=HIGH` 选择右声道。该资料是模块级商品手册而非完整的 MS4030 原厂硅片数据手册，所以登记为 `PUBLIC_GENERIC_CANDIDATE`。来源：<https://ae-pic-a1.aliexpress-media.com/kf/Saf3cf26844e744a6a2f9c48ba38f6ff9k.pdf>

据此，MS4030 的软件候选可先按 I²S RX、24-bit 有效数据、`L/R` 单声道选择处理；标准 I²S 的数据延迟、slot 宽度、采样率、BCLK 边沿和 ESP32 接收容器宽度仍需通过实物波形/有效数据验证。

### 已知网络

| 信号 | GPIO/连接 |
| --- | --- |
| SCK | GPIO9，与 MAX98357A 共享时钟 |
| WS | GPIO10，与 MAX98357A 共享 WS |
| SD | GPIO14 |
| VDD | 候选 3V3，实物待确认 |
| GND | GND |
| L/R | 选择脚状态未确认 |

### I²S 格式是什么意思

I²S 不只是三根线的名称。驱动还必须知道音频数据如何排列，包括：

- `sample rate`：每秒采样次数，例如 16 kHz 或 48 kHz；
- `word length`：有效采样位数，例如 16/24 bit；
- `slot width`：每个左右声道在总线中占用的位数，常见为 16/24/32 bit；
- `data alignment`：标准 I²S、左对齐或其他时序；
- `WS/LRCLK` 的左右声道含义；
- 数据在 BCLK 上升沿还是下降沿采样。

如果这些值不匹配，可能出现无声、噪声、左右声道错位或数据只有高位有效。因此当前只登记总线引脚，不替 MS4030 或 MAX98357A 猜测最终 I²S 配置。

### 未定义参数

- 实际 IC/产品标识：`MS4030`，Operator 已确认；公开资料只提供上述模块级通用参数；
- VDD 范围和输出电平；
- I²S 数据格式、采样率、slot 宽度、左右声道和时钟边沿；有效位宽的公开候选为 24-bit；
- L/R 默认通道及电阻配置；
- 麦克风时钟上限、噪声、增益和数据有效性；
- 机械声孔方向和安装影响。

所需驱动：ESP32 I²S RX、DMA、采样格式/通道配置和数据有效性检查。

## 8. MPU6500 IMU

### 已知/候选

- 接口：I²C，Operator 已确认；
- `MPUINT → GPIO21`；
- 是否与 PCA9685 共享 I²C 总线、以及最终 SDA/SCL 实物网络，仍待确认；
- 当前决策只把 IMU 作为可选记录/保护能力，不进入核心单腿 FSM。

### 未定义参数

- 实际模块供电电压和逻辑电平；
- I²C 7-bit 地址和 AD0 状态；
- SDA/SCL 的最终 GPIO/实物网络，以及是否与 PCA9685 共用总线；
- WHO_AM_I、量程、采样率、滤波、坐标方向、安装姿态；
- INT 极性和中断映射；
- 是否本阶段启用。

所需驱动：仅在 Operator 决定启用后编写总线、初始化、校准和采样驱动。

## 9. ESP32、I²C 和电源

### YD-ESP32-S3 开发板资料已读

| 参数 | 当前登记 |
| --- | --- |
| 开发板 | `YD-ESP32-S3-COREBOARD V1.4`；原厂资料不随最终最小源码分支发布 |
| 核心模块 | `ESP32-S3-WROOM-1` |
| USB/UART | 双 USB-C；原理图标出 `CH343P` USB-UART 桥和 USB-JTAG/USB-OTG 相关网络 |
| 3.3 V | 板上 `CJ6107A33GW` 线性稳压器产生 `VDD33`；ESP32 逻辑供电登记为 3.3 V |
| EN/BOOT | `CHIP_PU/EN` 有 10 kΩ 上拉；`GPIO0` 由 BOOT/自动下载电路控制 |
| 排针 | `J1/J2`，2×22；尺寸图给出排针间距约 25.40 mm、板宽约 27.94 mm、板长约 62.9 mm |
| Flash/PSRAM 容量 | 原理图未冻结具体容量；归档包内的 N16R8 下载文件不能证明当前实物容量，仍为 TBD |

该资料可以替代“开发板型号/3.3 V 供电/启动电路未知”的阻塞，但不能覆盖 Carrier Gerber 的实际网络，也不能确定当前焊上的 ESP32-S3-WROOM-1 内存变体。

| 参数 | 当前状态 |
| --- | --- |
| ESP32 | 开发板资料确认 `YD-ESP32-S3-COREBOARD V1.4` / `ESP32-S3-WROOM-1`；实际 Flash/PSRAM 变体仍 TBD |
| PCA I²C | Gerber：GPIO11 SDA、GPIO12 SCL |
| PCA I²C 频率 | 未定义；说明书支持的具体运行速率未作为当前系统参数冻结 |
| I²C 上拉 | Gerber/模块组合的实际上拉来源和阻值未确认 |
| 3V3 | ESP32 供电为 3.3 V；Carrier 由 LM2596S 产生，仍需实测启动、负载、纹波和舵机扰动 |
| 舵机电源 | 外部 6 V，经外部 10 A 保险后进入 Carrier/PCA V+ |
| 软件启动顺序 | 未实现；应先保持 OE 禁止，再初始化总线和安全 PWM |
| 硬件急停 | 当前不是电子断电；仍依赖物理断开 XT30/舵机电源 |

## 10. 舵机与运动学参数

参数化 PCA9685/舵机寄存器适配器已实现并有 mock 单元测试。通用默认值集中在 `firmware/platform/defaults.py`：50 Hz、500/1500/2500 µs、0°、正方向、±90° 和 CH0–CH7 顺序映射；这些值标记为 `GENERIC_DEFAULT`，可覆盖，不是实测值。默认 `ServoCalibration()` 仍为 TBD，只有显式调用 `generic_default(channel)` 才会采用候选标定；实体接入前必须用登记表中的实测值覆盖。

- 舵机型号/批次：候选 MG90S，尚未冻结；
- PCA 通道 → 腿/关节：未定义；
- 每个关节的零位、方向、脉宽、角度范围：未定义；
- 机械限位、连杆尺寸、安装偏置、关节顺序：未定义；
- `lift/swing/search/support` 到关节目标的映射：未定义；
- 断联、I²C 故障、传感器异常时的安全姿态：未定义。

## 11. 现有软件接口边界

- `firmware/control/` 是硬件无关的 Python FSM/mock 层；
- `firmware/platform/pca9685.py` 是本次新增的参数化 PCA9685/舵机协议适配器，I²C 总线通过接口注入；
- `firmware-esp32/` 现已恢复为可编辑的 ESP-IDF 应用源码树；其中的 native 组件是第一版可替换实现，尚未因存在源码而自动成为实物验证结论；Python 适配器仍只用于协议/mock 验证。
- 原生组件已覆盖 PCA9685、ST7789、通用 I²S（MAX98357A/MS4030）、MPU6500 和 KW10 输入；I²C、I²S、SPI、GPIO 的具体接线及任务启动顺序仍由配置和实测收敛。
- Python `firmware/control/` 目前是硬件无关的单腿 FSM 和 mock，不是实体驱动；
- Android BLE 只发送四腿按住状态，不发送 TFT、音频、PCA 或舵机命令；
- ESP32 BLE/NimBLE 端点源码和构建工程已进入 `firmware-esp32/`；当前新增外设组件仍需 native clean build 和实物验证。
- 任何实体驱动都必须先接入 `LegActuator`、`FootContactSensor`、`PlayerInput` 或新增明确的 capability adapter；
- BLE watchdog、OE 安全态、舵机电源断开和实体急停不能互相冒充。

## 12. 当前信息完备性判断

### 通用第一轮默认值（均可覆盖）

以下值按照“先尝试常见配置、失败后替换”的开发守则登记为 `GENERIC_DEFAULT`，不等于实物确认：

| 子系统 | 全局默认候选 |
| --- | --- |
| ST7789 | SPI mode 0、8 MHz、RGB565 (`0x55`)、`MADCTL=0x00`、X offset 35、Y offset 0、背光逻辑高有效 |
| I²S | 48 kHz、32-bit slot、标准 I²S、上升沿采样；MAX98357A 有效数据 16-bit，MS4030 有效数据 24-bit |
| MAX98357A | 左声道候选；实际 SD_MODE/GAIN 仍按模块确认 |
| MS4030 | L/R 低电平选择左声道 |
| KW10 | GPIO 仍 TBD；内部/外部上拉候选、低电平有效、20 ms 去抖 |
| MPU6500 | I²C 地址 `0x68`、AD0 低电平；是否共用 PCA 总线仍 TBD |

Python mock 的对应全局默认值位于 [`firmware/platform/defaults.py`](../../firmware/platform/defaults.py)；ESP-IDF 工程的可烧录配置候选位于 [`firmware-esp32/sdkconfig.defaults`](../../firmware-esp32/sdkconfig.defaults)，各 native component 头文件中的默认宏与本表保持同一候选语义。

| 子系统 | 当前是否足以写协议/接口代码 | 仍需补全后才能安全接实物 |
| --- | --- | --- |
| PCA9685 总线 | 是：地址/寄存器协议已有；本次已加入参数化适配器 | 实物地址跳线、VCC/OE 上电行为、I²C 上拉、舵机 V+ 压降/温升 |
| 舵机 | 是：可写拒绝 TBD 标定值的通用适配器 | 通道、频率、最小/中位/最大脉宽、零位、方向、软件/机械限位、安全姿态 |
| ESP32 开发板 | 是：型号、核心、3.3 V、USB/下载电路、尺寸和 ESP-IDF 源码基座已具备 | 当前 WROOM-1 的 Flash/PSRAM 变体；native clean build/烧录后的硬件验证 |
| KW10 足端 | 否，仅能登记网络 | FOOT GPIO、FL/FR/RL/RR 对应、always-on/off、去抖和触地逻辑 |
| ST7789 | 是：已写通用 SPI 初始化/窗口/像素传输组件 | 实物初始化表、X/Y offset、MADCTL、背光电压/电流和有效电平 |
| MAX98357A | 是：已写通用 I²S TX 组件（MAX98357A 作为标准 I²S sink） | 模块实际 A/B 芯片、SD_MODE/GAIN、采样率/slot、扬声器和静音行为 |
| MS4030 | 是：已写通用 I²S RX 配置候选 | 实物 VDD/L-R、数据 slot/边沿/采样率、波形和 ESP32 接收容器 |
| MPU6500 | 是：已写 I²C 设备初始化、WHO_AM_I 和原始采样组件 | 地址/AD0、是否共享 PCA 总线、最终 SDA/SCL、量程/滤波/安装姿态 |

结论：信息已经足以开始参数化底层驱动代码和 mock/native 编译验证；当前实现不冻结任何未定义硬件参数，也不代表 TFT、MAX98357A、MS4030、KW10、MPU6500 或舵机已经完成实物验收。下一步是 clean build、烧录前配置复核和逐模块台架验证。

## 13. 实测记录模板

| 日期 | 模块/网络 | 测试目的 | 接线/供电 | 仪器 | 结果 | 原始证据 |
| --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |

## 14. Operator 确认

确认本文件中的 Gerber GPIO 映射和参数状态后，再填写：

```text
Operator：________________
日期：____________________
允许开始的驱动范围：____________________
仍禁止的驱动范围：______________________
```
