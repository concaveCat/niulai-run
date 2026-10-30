import json
import unittest

from services.feedback_proxy.server import (
    FeedbackProxyError,
    build_messages,
    generate_feedback,
    normalize_feedback,
    validate_payload,
)


def sample_payload():
    return {
        "locale": "zh-CN",
        "target_length": "about_100_zh_chars",
        "game": {
            "mode": "SOLO",
            "elapsed_ms": 12000,
            "progress_percent": 72,
            "checkpoint": 2,
            "speed_score": 68,
            "coordination_score": 81,
            "event_count": 9,
            "leg_press_counts": {"FL": 3, "FR": 2, "RL": 2, "RR": 2},
            "rhythm_consistency_percent": 84,
            "highlight": "换腿节奏稳定，配合很清楚",
            "suggestion": "尝试减少停顿",
        },
    }


class FakeResponse:
    status = 200

    def __init__(self, body):
        self.body = body

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def read(self):
        return json.dumps(self.body, ensure_ascii=False).encode("utf-8")


class FeedbackProxyTest(unittest.TestCase):
    def test_validate_rejects_missing_aggregate(self):
        payload = sample_payload()
        del payload["game"]["event_count"]
        with self.assertRaises(FeedbackProxyError):
            validate_payload(payload)

    def test_normalize_accepts_target_length_and_removes_prefix(self):
        text = "AI评价：本局节奏稳定，四腿输入衔接自然，推进过程清晰。建议继续保持换腿间隔，并逐步减少停顿，让整体动作更加连贯。"
        self.assertTrue(normalize_feedback(text).startswith("本局节奏稳定"))

    def test_generate_feedback_sends_only_proxy_request(self):
        captured = {}

        def opener(request, timeout):
            captured["headers"] = dict(request.headers)
            captured["body"] = json.loads(request.data.decode("utf-8"))
            captured["timeout"] = timeout
            return FakeResponse({
                "choices": [{"message": {"content": "本局节奏稳定，四腿输入衔接自然，推进过程清晰。建议继续保持换腿间隔，并逐步减少停顿，让整体动作更加连贯。"}}],
            })

        result = generate_feedback(
            sample_payload(),
            env={"LLM_API_KEY": "test-key", "LLM_MODEL": "test-model", "LLM_BASE_URL": "http://proxy/v1"},
            opener=opener,
        )
        self.assertIn("本局节奏稳定", result)
        self.assertEqual("Bearer test-key", captured["headers"]["Authorization"])
        self.assertEqual("test-model", captured["body"]["model"])
        self.assertEqual(8.0, captured["timeout"])
        self.assertNotIn("timestamp", json.dumps(captured["body"], ensure_ascii=False))

    def test_missing_credentials_is_safe_error(self):
        with self.assertRaisesRegex(FeedbackProxyError, "LLM_API_KEY"):
            generate_feedback(sample_payload(), env={})

    def test_fall_feedback_prompt_forbids_success_praise(self):
        payload = sample_payload()
        payload["game"].update({
            "failure_reason": "FALL",
            "speed_score": 0,
            "coordination_score": 0,
        })
        system = build_messages(payload)[0]["content"]

        self.assertIn("摔倒失败", system)
        self.assertIn("速度分0分", system)
        self.assertIn("不要夸奖", system)

    def test_validate_rejects_unknown_failure_reason(self):
        payload = sample_payload()
        payload["game"]["failure_reason"] = "OTHER"
        with self.assertRaises(FeedbackProxyError):
            validate_payload(payload)


if __name__ == "__main__":
    unittest.main()
