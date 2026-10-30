# Contributing to 牛来快跑

感谢你愿意参与。牛来快跑同时包含固件、Android、硬件和大量台架记录，所以最重要的规则不是“代码风格”，而是**不要把未验证的东西写成已验证**。

## 开始之前

1. 阅读根目录 [`README.md`](README.md)，尤其是[安全边界](README.md#安全边界)。
2. 阅读与改动相关的协议、Spec、Decision 或 Experiment。
3. 确认改动属于哪一类：
   - 事实/观测；
   - 假设/候选；
   - 提议；
   - 已接受决定。
4. 涉及实体动作、电源、保护阈值或急停的改动，先做 mock/host 验证，并标记为 **HIGH RISK**。

## 开发流程

1. 从最新 `main` 创建短生命周期分支，例如：
   - `fw/<topic>`
   - `android/<topic>`
   - `hw/<topic>`
   - `docs/<topic>`
   - `experiment/<id>`
2. 一个 PR 只解决一个可描述的问题。
3. 行为变更必须同时更新测试和文档。
4. 提交前运行与风险相称的检查，并在 PR 中写清楚**准确命令和结果**。
5. 通过 PR 合并；不要让 `main` 长期处于不可构建状态。

## 必须运行的检查

无硬件：

```bash
python3 -m pip install -r requirements-dev.txt
sh tools/run_offline_checks.sh
```

Android（有 JDK/Android SDK 时）：

```bash
cd android-controller
./gradlew testDebugUnitTest
./gradlew assembleDebug
```

固件构建（有 ESP-IDF 时）：

```bash
cd firmware-esp32
idf.py set-target esp32s3
idf.py build
```

如果某项检查没有运行，请在 PR 中写明原因和风险，不要用“应该能跑”代替。

## 代码约定

- 控制层保持硬件无关；具体器件通过 adapter/capability 接入。
- 纯 C 控制/协议核心不得引入 ESP-IDF/FreeRTOS 依赖；host suite 会检查。
- 新增行为必须有测试；bug fix 先用测试或可复现步骤证明问题。
- 不要删除、弱化或跳过测试来“修复”CI。
- 不要提交 `build/`、`sdkconfig`、`.gradle/`、APK、日志垃圾或本地路径。
- 不要提交 API Key、token、个人数据、SSH 私钥或未授权素材。
- 生成代码和手写代码遵守同一标准；不要提交自己无法解释的内容。

## 文档约定

- 新增/改变用户可见行为、接口、硬件能力或构建方式时，同步更新 `README.md` 和相关文档。
- 重大方案变化写 Decision Record；可复现实验写 Experiment Record。
- 实验记录要包含硬件版本、参数、软件版本、原始日志位置、重复次数和失败结果。
- 已被取代的决定保留历史，用 `Superseded` 链接后继记录，不要静默删除。

## 实体硬件改动

任何会改变运动范围、力矩、速度、保护阈值、电源或急停行为的 PR：

- 必须标记 `HIGH RISK`；
- 必须先完成 mock/host 验证；
- 必须写明台架条件：机械固定、限位、6 V 限流、10 A 保险、人员净空、可立即断开 XT30；
- 必须由具备安全条件的人执行实体测试，并记录结果；
- 不允许把“构建成功”或“串口无报错”写成“实体动作已验收”。

## 提交信息

建议使用简短、可追溯的主题，例如：

```text
fw: reject out-of-envelope PCA9685 frames
android: keep rematch mode after result
docs: clarify BLE_BONDED vs encrypted link
experiment: EXP-044 low-volume audio A/B
```

PR 描述请包含：

- 改了什么、为什么；
- 事实/假设/决定分别是什么；
- 测试命令与结果；
- 剩余风险；
- 如何回滚或安全禁用。

## 安全

涉及电源、舵机、急停、机械伤害或人身安全的问题，请先阅读 [`SECURITY.md`](SECURITY.md)，不要公开利用细节。

## 许可证

提交即表示你同意以本仓库的 MIT License 授权你的原创贡献；第三方组件和素材仍适用各自许可证，见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。
