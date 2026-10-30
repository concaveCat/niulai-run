package io.niulai.controller.feedback

import io.niulai.controller.gameplay.GameplayFeedbackSummary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URI
import java.net.URL

interface GameplayFeedbackProvider {
    suspend fun generate(summary: GameplayFeedbackSummary): Result<String>
}

class HttpGameplayFeedbackProvider(
    private val baseUrl: String,
    private val apiKey: String = "",
    private val model: String = "",
    private val useProxy: Boolean = false,
    private val connectTimeoutMs: Int = 8_000,
    private val readTimeoutMs: Int = 8_000,
) : GameplayFeedbackProvider {
    fun debugDescription(): String {
        val directApi = !useProxy
        val endpoint = gameplayFeedbackEndpoint(baseUrl, directApi)
        val mode = if (directApi) "直连 API" else "本地代理"
        val modelName = model.trim().ifBlank { "未填写" }
        return "模式=$mode；地址=${safeFeedbackUrl(endpoint)}；模型=$modelName"
    }

    override suspend fun generate(summary: GameplayFeedbackSummary): Result<String> =
        withContext(Dispatchers.IO) {
            runCatching {
                val directApi = !useProxy
                if (directApi && apiKey.isBlank()) {
                    error("未填写 API Key，请在设置页填写 DeepSeek API Key")
                }
                if (directApi && model.isBlank()) error("未填写模型名称")
                val endpoint = gameplayFeedbackEndpoint(baseUrl, directApi)
                val connection = (URL(endpoint)
                    .openConnection() as HttpURLConnection)
                try {
                    connection.requestMethod = "POST"
                    connection.connectTimeout = connectTimeoutMs
                    connection.readTimeout = readTimeoutMs
                    connection.doOutput = true
                    connection.setRequestProperty("Content-Type", "application/json")
                    connection.setRequestProperty("Accept", "application/json")
                    if (directApi) {
                        connection.setRequestProperty("Authorization", "Bearer ${apiKey.trim()}")
                    }
                    connection.outputStream.use { output ->
                        val requestBody = if (directApi) {
                            summary.toChatCompletionJson(model.trim())
                        } else {
                            summary.toRequestJson()
                        }
                        output.write(requestBody.toByteArray(Charsets.UTF_8))
                    }

                    val status = connection.responseCode
                    val stream = if (status in 200..299) {
                        connection.inputStream
                    } else {
                        connection.errorStream
                    }
                    val body = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
                    if (status !in 200..299) {
                        val detail = body
                            .replace(Regex("\\s+"), " ")
                            .trim()
                            .let(::redactFeedbackDebug)
                            .take(240)
                        val suffix = if (detail.isBlank()) "" else ": $detail"
                        error("评价服务 HTTP $status$suffix")
                    }

                    val response = JSONObject(body)
                    val content = if (directApi) {
                        response.optJSONArray("choices")
                            ?.optJSONObject(0)
                            ?.optJSONObject("message")
                            ?.optString("content")
                            .orEmpty()
                    } else {
                        response.optString("feedback")
                    }
                    val feedback = FeedbackTextValidator.normalize(content)
                        ?: error("feedback service returned invalid text")
                    feedback
                } finally {
                    connection.disconnect()
                }
            }
        }
}

internal fun safeFeedbackUrl(value: String): String {
    return runCatching {
        val uri = URI(value)
        buildString {
            uri.scheme?.let {
                append(it)
                append("://")
            }
            append(uri.host ?: "未知主机")
            if (uri.port >= 0) append(":${uri.port}")
            append(uri.path.orEmpty())
        }
    }.getOrElse {
        value.substringBefore('?').substringBefore('#').take(180)
    }
}

private fun redactFeedbackDebug(value: String): String = value.replace(
    Regex("(?i)(\\\"?(?:api[_ -]?key|authorization|token|secret)\\\"?\\s*[:=]\\s*\\\"?)[^\\\",}\\s]+"),
    "$1[redacted]",
)

object FeedbackTextValidator {
    const val MAX_CHARS = 160
    private const val MIN_CHARS = 20

    fun normalize(value: String): String? {
        val cleaned = value
            .replace(Regex("```(?:text|markdown)?"), "")
            .replace(Regex("^\\s*(评价|AI评价|专属评价)\\s*[:：]\\s*"), "")
            .replace(Regex("\\s+"), " ")
            .trim()
        return cleaned.takeIf { it.length in MIN_CHARS..MAX_CHARS }
    }
}

internal fun gameplayFeedbackEndpoint(value: String, directApi: Boolean): String {
    val normalized = value
        .trim()
        .substringBefore('?')
        .substringBefore('#')
        .trimEnd('/')
    return if (directApi) {
        if (normalized.endsWith("/chat/completions")) normalized else "$normalized/chat/completions"
    } else {
        "$normalized/v1/gameplay-feedback"
    }
}

private fun GameplayFeedbackSummary.toRequestJson(): String = JSONObject().apply {
    put("locale", "zh-CN")
    put("target_length", "about_100_zh_chars")
    put("game", JSONObject().apply {
        put("mode", mode.name)
        put("elapsed_ms", elapsedMs)
        put("progress_percent", progressPercent)
        put("checkpoint", checkpoint)
        put("completed_steps", completedSteps)
        put("speed_score", speedScore ?: JSONObject.NULL)
        put("coordination_score", coordinationScore)
        put("event_count", eventCount)
        put("leg_press_counts", JSONObject().apply {
            put("FL", legPressCounts.getOrElse(0) { 0 })
            put("FR", legPressCounts.getOrElse(1) { 0 })
            put("RL", legPressCounts.getOrElse(2) { 0 })
            put("RR", legPressCounts.getOrElse(3) { 0 })
        })
        put("rhythm_consistency_percent", rhythmConsistencyPercent)
        put("highlight", highlight)
        put("suggestion", suggestion)
        put("failure_reason", failureReason?.name ?: JSONObject.NULL)
    })
}.toString()

private fun GameplayFeedbackSummary.toChatCompletionJson(model: String): String = JSONObject().apply {
    put("model", model)
    put("temperature", 0.7)
    put("max_tokens", 180)
    put("messages", JSONArray().apply {
        put(JSONObject().apply {
            put("role", "system")
            put(
                "content",
                if (failureReason != null) {
                    "你是四足机器人协作游戏教练。本局已因摔倒失败，必须明确说明失败原因和速度分0分、协调分0分。" +
                        "不要夸奖本局表现，不要声称完成了有效动作，不要提出与输入矛盾的成绩；只给出简短安全建议。"
                } else {
                    "你是四足机器人协作游戏教练。请根据本局汇总数据，用简体中文写一段约100字的赛后评价。" +
                        "必须包含一个具体优点和一个下一步建议，只能使用输入中的事实，不得虚构设备状态、动作或成绩。"
                } +
                    "不要使用Markdown、标题、列表或客套开场，长度控制在80到120个汉字左右，最多160字。",
            )
        })
        put(JSONObject().apply {
            put("role", "user")
            put("content", toRequestJson())
        })
    })
}.toString()

fun localGameplayFeedback(summary: GameplayFeedbackSummary): String {
    if (summary.failureReason != null) {
        return "本局因检测到机体摔倒而失败，速度分0分，人类协调分0分。请重新站稳后再来一局。"
    }
    val speed = summary.speedScore?.let { "速度分${it}分" } ?: "本局暂无速度分"
    val steps = "有效步${summary.completedSteps}步"
    return "本局${steps}，${speed}，协调分${summary.coordinationScore}分。${summary.highlight}。建议${summary.suggestion}。"
}
