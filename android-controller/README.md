# Android 控制器（android-controller）

牛来快跑手机端，应用名为 **“牛来快跑”**。它是一个 Kotlin + Jetpack Compose 单 Activity 应用，承担：

- BLE Central / GATT Client；
- 四腿触控输入与 100 ms 心跳；
- 单人 / 双人模式；
- 有效步评分与结算；
- KW10 触地、FAILSAFE、摔倒失败的显示；
- HOME 安全归位与断开收尾；
- 可选的 AI 赛后评价；
- 完全离线的 Test Ctrl。

> 固件默认是 `BLE_ONLY` 安全档，因此“能连上 App”不等于“舵机会动”。实体动作需要显式构建台架固件，并满足根目录 README 的安全条件。

---

## 1. 技术栈

| 项目 | 版本/说明 |
| --- | --- |
| 语言 | Kotlin 2.2.20 |
| UI | Jetpack Compose（BOM 2025.09.00）+ Material 3 |
| 构建 | Gradle 8.13 + AGP 8.13.2 |
| JDK | 17 |
| compileSdk / targetSdk | 36（仅构建配置） |
| 运行要求 | Android 8.0+ |
| minSdk | 26（Android 8.0） |
| 架构 | 单 Activity + `ControllerViewModel` + `StateFlow` |
| BLE | Android Bluetooth LE GATT |
| 测试 | JUnit4 + kotlinx-coroutines-test |

---

## 2. 目录结构

```text
app/src/main/java/io/niulai/controller/
├── MainActivity.kt              权限、生命周期、Compose 入口
├── NiulaiApplication.kt       应用级日志/初始化
├── ble/
│   ├── BleController.kt         BLE 抽象接口与状态模型
│   └── AndroidBleController.kt  扫描、连接、GATT、通知、写入
├── control/
│   └── LegInputTracker.kt       多指触控 → 四腿掩码/序号
├── protocol/
│   └── BleProtocol.kt           UUID、4 字节控制包、STATUS、IMU、HOME、AUDIO
├── gameplay/
│   └── GameplaySession.kt       会话、事件、有效步、双评分、AI 汇总
├── feedback/
│   ├── GameplayFeedback.kt      AI 评价 provider / 本地回退
│   └── GameplayFeedbackSettings.kt 设置与 Keystore 加密
└── ui/
    ├── ControllerScreen.kt      所有页面与组件
    └── ControllerViewModel.kt   状态编排、心跳、HOME、游戏流程

app/src/test/java/...            单元测试
app/src/main/res/                图片、字体、主题、字符串
```

---

## 3. 页面与流程

```text
Main
├─ 开始 → Connect → Ctrl（实体）
│                    ├─ 单人 / 双人
│                    ├─ 四腿按住控制
│                    ├─ 牛来（播放音频）
│                    ├─ 归位（HOME）
│                    └─ 结束 → Result → 再来一局 / 返回主页
├─ 不连接设备，进入测试 → Test Ctrl（模拟）
└─ 设置 → AI 评价服务
```

- **Main**：封面、开始、无设备测试、设置。
- **Connect**：扫描、设备列表、连接状态/错误。
- **Ctrl**：实体控制页；显示 BLE、STATUS、触地、动态加速度、计时和四个腿按钮。
- **Test**：不连接 BLE 的离线控制/评分演示。
- **Result**：速度分、协调分、用时、步数、AI 评价、再来一局。
- **Settings**：API URL、API Key、模型、是否使用本地代理。

支持横屏/竖屏布局；控制按钮位置按语义 `FL/FR/RL/RR` 排布，BLE 掩码不随屏幕方向改变。

---

## 4. BLE 交互

### 4.1 扫描与连接

- 应用 ID：`io.niulai.controller`；BLE 设备名：`牛来快跑`；
- 按服务 UUID 或设备名 `牛来快跑` 过滤扫描结果；
- 扫描超时 10 s；
- GATT 连接/服务发现/STATUS 就绪超时 12 s；
- 连接后要求 `CONTROL_STATE`、`STATUS`、`HOME_COMMAND`、`IMU_TELEMETRY` 等必需特征存在；
- 订阅 STATUS 和 IMU 通知，并完成初始 STATUS 读取后才报告 `CONNECTED`；
- 写入首个加密绑定的中立控制包成功后才进入可操作状态。

### 4.2 控制包

```text
byte 0: version = 0x01
byte 1: sequence
byte 2: leg mask (bit 0..3 = FL/FR/RL/RR)
byte 3: flags = 0x00
```

- 每次有效按下/松开/取消都会递增序号；
- 约 100 ms 重发最新完整包作为心跳；
- 重复序号的心跳会刷新固件 watchdog；
- 断连/退后台/离开控制页会尽力发送中立包，但 BLE 不保证送达，因此固件 500 ms watchdog 仍是最终保护。

### 4.3 HOME 与结束游戏

实体游戏结束时：

1. 停止心跳；
2. 清空本地按住状态；
3. 等待中立 `CONTROL_STATE` 写回调（最多 2 s）；
4. 发送加密 `HOME_COMMAND`；
5. 等待 `STATUS.HOME_COMPLETE`（最多 1.5 s）；
6. 主动断开；固件断开钩子执行 `/OE` 高和 PCA9685 safe-stop。

即使 HOME 通知丢失，也会在有界等待后断开，避免舵机持续带电。

### 4.4 游戏模式与音频

- `GAME_MODE`：`0x00` 单人、`0x01` 双人；只改变 TFT 场景，不改变控制包/FSM/安全门。
- `AUDIO_COMMAND`：`0x01` 播放内置 `牛来叫妈妈.mp3`；只走音频路径，不触发运动。

完整协议见 [`../docs/BLE_PROTOCOL.md`](../docs/BLE_PROTOCOL.md)。

---

## 5. 评分与反馈

### 有效步

- **Test 模式**：按下—松开即模拟一步。
- **实体模式**：必须满足“固件已应用 → 离地 → 释放 → 重新触地”才计一步。
- 少于 3 步显示“试跑数据”，不输出正式分数。
- 摔倒失败：速度分 0、协调分 0。

### 候选公式

```text
速度分 = 70% 有效步节奏 + 20% 连贯性 + 10% 有效步数量
协调分 = 40% 动作完成率 + 25% 固件接纳率
       + 20% 节奏稳定性 + 15% 四腿参与度
```

权重、时间窗口和触地判定仍是 Testing，需要实体数据校准。评分只观察输入/STATUS，不向执行器写任何东西。

### AI 赛后评价

- 默认使用 DeepSeek / OpenAI 兼容 Chat Completions；
- 可在设置中填写 API URL、模型和 API Key；
- API Key 使用 Android Keystore（AES-GCM）加密保存，不写日志；
- 可选择本地 `feedback_proxy`，避免把 Key 放进 APK；
- 网络失败/无 Key/响应不合法时自动回退到本地确定性评价，不影响计分和重开。

代理说明见 [`../services/feedback_proxy/README.md`](../services/feedback_proxy/README.md)。

---

## 6. 构建

要求：

- JDK 17
- 本机可用的 Android SDK。项目构建配置使用 `compileSdk 36`，Android Studio 会自动安装对应平台；运行 APK 只需要 Android 8.0+。
- 可访问 Google Maven / Maven Central

```bash
cd android-controller
./gradlew testDebugUnitTest
./gradlew assembleDebug
```

APK：

```text
app/build/outputs/apk/debug/app-debug.apk
```

如果使用本地评价代理，可在构建时覆盖默认地址：

```bash
./gradlew assembleDebug \
  -PgameplayFeedbackBaseUrl=http://192.168.1.20:8787
```

也可以直接在 App 的“设置”页修改，不需要重新构建。

---

## 7. 测试

```bash
cd android-controller
./gradlew testDebugUnitTest
```

覆盖：

- BLE 包编码/解码、保留位拒绝、序号回绕；
- HOME、IMU、AUDIO、GAME_MODE 校验；
- 多指输入跟踪、取消、断连清零；
- ViewModel 的首个中立包、心跳、HOME 顺序、FAILSAFE 清零；
- 单人/双人会话、有效步、重开模式、页面路由；
- 反馈文本校验和本地回退。

单元测试不需要手机或机器人。真实 BLE 配对、通知时序、Android 厂商兼容性、实体 HOME 和音频仍需真机+台架验收。

---

## 8. 安全与隐私

- 不在仓库、Gradle 参数、APK 或截图中写入真实 API Key。
- API Key 使用 Android Keystore 加密；仍然建议使用权限受限、可随时轮换的测试 Key。
- 本地代理默认只监听 `127.0.0.1`，不要把无认证代理暴露到公网。
- 实体控制页在 `FAILSAFE` 或 `FALL_DETECTED` 时禁用腿按钮，并清空本地输入。
- 断连、退后台、旋转/生命周期变化都会清理按住状态；不能依赖“最后一次写入”保持动作。
- `Test Ctrl` 完全离线，不会连接或驱动硬件。

---

## 9. 已知限制

- 应用 ID 为 `io.niulai.controller`，BLE 名称为 `牛来快跑`。如果从旧版包升级，请先卸载旧应用并清除系统蓝牙中的旧配对，避免旧 GATT 缓存/旧 bond 干扰。
- BLE 台架 profile 无 MITM/PIN；不是生产级身份认证。
- 部分 Android 厂商的 GATT 缓存/通知行为可能不同，首次连接失败可尝试系统蓝牙设置中取消配对后重连。
- 评分权重和触地窗口仍是候选值。
- AI 评价是可选增强，不是游戏核心；无网络时使用本地评价。
- 控制页的横屏布局需要真机旋转/尺寸验收。
