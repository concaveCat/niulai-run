package io.niulai.controller.feedback

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertNull
import org.junit.Test

class FeedbackTextValidatorTest {
    @Test
    fun buildsDirectAndProxyEndpoints() {
        assertEquals(
            "https://api.example.test/v1/chat/completions",
            gameplayFeedbackEndpoint("https://api.example.test/v1/", directApi = true),
        )
        assertEquals(
            "http://10.0.2.2:8787/v1/gameplay-feedback",
            gameplayFeedbackEndpoint("http://10.0.2.2:8787", directApi = false),
        )
    }

    @Test
    fun removesKnownPrefixAndMarkdownFence() {
        assertEquals(
            "本局节奏稳定，建议继续保持换腿间隔并减少停顿。",
            FeedbackTextValidator.normalize("```text\nAI评价：本局节奏稳定，建议继续保持换腿间隔并减少停顿。\n```") ,
        )
    }

    @Test
    fun rejectsEmptyAndOverlongText() {
        assertNull(FeedbackTextValidator.normalize(""))
        assertNull(FeedbackTextValidator.normalize("a".repeat(FeedbackTextValidator.MAX_CHARS + 1)))
    }

    @Test
    fun debugDescriptionUsesDeepSeekChatEndpointWithoutExposingKey() {
        val provider = HttpGameplayFeedbackProvider(
            baseUrl = "https://api.deepseek.com/v1?api_key=should-not-display",
            apiKey = "sk-test-secret",
            model = "deepseek-chat",
        )

        val debug = provider.debugDescription()

        assertTrue(debug.contains("模式=直连 API"))
        assertTrue(debug.contains("https://api.deepseek.com/v1/chat/completions"))
        assertTrue(debug.contains("模型=deepseek-chat"))
        assertFalse(debug.contains("sk-test-secret"))
        assertFalse(debug.contains("api_key=should-not-display"))
    }

    @Test
    fun proxyDebugDescriptionUsesProxyEndpoint() {
        val provider = HttpGameplayFeedbackProvider(
            baseUrl = "http://10.0.2.2:8787",
            model = "deepseek-chat",
            useProxy = true,
        )

        assertEquals(
            "模式=本地代理；地址=http://10.0.2.2:8787/v1/gameplay-feedback；模型=deepseek-chat",
            provider.debugDescription(),
        )
    }
}
