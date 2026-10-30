# 赛后评价代理（feedback_proxy）

一个只依赖 Python 标准库的本地 HTTP 代理，用于把 Android 端本局汇总指标转发给 OpenAI 兼容的 Chat Completions 接口。它的目标是**把 API Key 留在电脑/服务端，不放进 APK**。

代理不会记录请求正文，也不会把 Key 写进日志。Android 端在代理不可用、无 Key 或响应不合法时会回退到本地确定性评价，不影响计分、重开或控制。

---

## 1. 启动

在仓库根目录：

```bash
LLM_BASE_URL=https://api.openai.com/v1 \
LLM_MODEL=your-model \
LLM_API_KEY=replace-me \
python3 -m services.feedback_proxy.server
```

默认监听 `127.0.0.1:8787`，接口：

```text
POST /v1/gameplay-feedback
```

Android 模拟器中的 `http://10.0.2.2:8787` 会映射到宿主机。实体手机需要把 App 的“评价服务设置”改成宿主机的局域网地址，或构建时覆盖：

```bash
cd android-controller
./gradlew assembleDebug -PgameplayFeedbackBaseUrl=http://192.168.1.20:8787
```

---

## 2. 配置

| 环境变量 | 默认值 | 说明 |
| --- | --- | --- |
| `LLM_BASE_URL` | `https://api.openai.com/v1` | 包含版本前缀的服务地址 |
| `LLM_MODEL` | 空 | 必填，模型名称 |
| `LLM_API_KEY` | 空 | 必填，服务端保存 |
| `LLM_CHAT_PATH` | `/chat/completions` | 兼容接口路径 |
| `LLM_TIMEOUT_SECONDS` | `8` | 上游超时 |
| `FEEDBACK_PROXY_HOST` | `127.0.0.1` | 监听地址 |
| `FEEDBACK_PROXY_PORT` | `8787` | 监听端口 |

---

## 3. 请求与响应

请求是本局汇总指标，不包含原始按键流、设备标识或个人数据。字段包括：

- 模式（`SOLO` / `SPLIT`）；
- 用时、进度、checkpoint、有效步数；
- 速度分、协调分、事件数、四腿按压次数、节奏一致性；
- 本地生成的亮点/建议摘要；
- 失败原因（可选 `FALL`）。

代理会校验字段类型和范围，并限制请求体 16 KB、输出 20–160 字符。上游返回无效内容时返回安全错误，Android 回退本地评价。

示例：

```bash
curl -s http://127.0.0.1:8787/v1/gameplay-feedback \
  -H 'Content-Type: application/json' \
  -d '{
    "locale": "zh-CN",
    "game": {
      "mode": "SOLO",
      "elapsed_ms": 32000,
      "progress_percent": 75,
      "checkpoint": 0,
      "completed_steps": 6,
      "speed_score": 72,
      "coordination_score": 68,
      "event_count": 14,
      "leg_press_counts": {"FL": 4, "FR": 3, "RL": 4, "RR": 3},
      "rhythm_consistency_percent": 70,
      "highlight": "节奏稳定",
      "suggestion": "尝试更均匀地释放"
    }
  }'
```

---

## 4. 测试

```bash
python3 -m unittest discover -s services/feedback_proxy/tests -p 'test_*.py' -v
```

测试不访问网络，覆盖请求校验、失败回退、输出清洗和 HTTP 处理。

---

## 5. 安全

- 不要把真实 API Key 写入仓库、Gradle 参数、APK 或截图。
- 代理默认只监听 `127.0.0.1`；不要暴露到公网。
- 如果必须局域网访问，请加防火墙/反向代理认证，并使用可轮换的受限 Key。
- 代理只返回短文本评价，不接收控制命令，也不能影响机器人运动。
