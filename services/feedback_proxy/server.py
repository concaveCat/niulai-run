#!/usr/bin/env python3
"""Small dependency-free proxy for privacy-safe gameplay feedback."""

from __future__ import annotations

import json
import os
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Callable, Mapping
from urllib.request import Request, urlopen

MAX_REQUEST_BYTES = 16 * 1024
MAX_FEEDBACK_CHARS = 160
MIN_FEEDBACK_CHARS = 20
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8787


class FeedbackProxyError(Exception):
    """An expected validation or upstream failure safe to expose to the client."""


class FeedbackPayloadError(FeedbackProxyError):
    """The Android request is malformed or outside the accepted aggregate schema."""


def normalize_feedback(value: str) -> str | None:
    cleaned = re.sub(r"^\s*(评价|AI评价|专属评价)\s*[:：]\s*", "", value or "")
    cleaned = re.sub(r"```(?:text|markdown)?", "", cleaned)
    cleaned = re.sub(r"\s+", " ", cleaned).strip()
    if MIN_FEEDBACK_CHARS <= len(cleaned) <= MAX_FEEDBACK_CHARS:
        return cleaned
    return None


def validate_payload(payload: Any) -> dict[str, Any]:
    if not isinstance(payload, dict) or not isinstance(payload.get("game"), dict):
        raise FeedbackPayloadError("request must contain a game object")
    game = payload["game"]
    required = (
        "mode",
        "elapsed_ms",
        "progress_percent",
        "checkpoint",
        "speed_score",
        "coordination_score",
        "event_count",
        "leg_press_counts",
        "rhythm_consistency_percent",
        "highlight",
        "suggestion",
    )
    missing = [name for name in required if name not in game]
    if missing:
        raise FeedbackPayloadError("missing game fields: " + ", ".join(missing))
    if game["mode"] not in ("SOLO", "SPLIT"):
        raise FeedbackPayloadError("invalid gameplay mode")
    for name in ("elapsed_ms", "progress_percent", "checkpoint", "coordination_score", "event_count", "rhythm_consistency_percent"):
        if not isinstance(game[name], int) or isinstance(game[name], bool):
            raise FeedbackPayloadError(f"{name} must be an integer")
    if game["elapsed_ms"] < 0 or game["progress_percent"] not in range(101):
        raise FeedbackPayloadError("invalid elapsed or progress value")
    if game["coordination_score"] not in range(101) or game["rhythm_consistency_percent"] not in range(101):
        raise FeedbackPayloadError("score values must be between 0 and 100")
    if game["event_count"] < 0 or game["checkpoint"] < 0:
        raise FeedbackPayloadError("event count and checkpoint cannot be negative")
    if game["speed_score"] is not None and (
        not isinstance(game["speed_score"], int) or game["speed_score"] not in range(101)
    ):
        raise FeedbackPayloadError("speed score must be null or between 0 and 100")
    counts = game["leg_press_counts"]
    if not isinstance(counts, dict) or set(counts) != {"FL", "FR", "RL", "RR"}:
        raise FeedbackPayloadError("leg_press_counts must contain FL, FR, RL and RR")
    if any(not isinstance(value, int) or value < 0 for value in counts.values()):
        raise FeedbackPayloadError("leg press counts must be non-negative integers")
    if not isinstance(game["highlight"], str) or not isinstance(game["suggestion"], str):
        raise FeedbackPayloadError("highlight and suggestion must be strings")
    if game.get("failure_reason") not in (None, "FALL"):
        raise FeedbackPayloadError("invalid failure reason")
    return payload


def build_messages(payload: Mapping[str, Any]) -> list[dict[str, str]]:
    game = payload["game"]
    if game.get("failure_reason") == "FALL":
        system = (
            "你是四足机器人协作游戏教练。本局已因机体摔倒失败，必须明确说明摔倒原因、速度分0分和协调分0分。"
            "不要夸奖本局表现，不要声称完成了有效动作，只给出简短安全建议。"
        )
    else:
        system = (
            "你是四足机器人协作游戏教练。请根据用户提供的本局汇总数据，用简体中文写一段约100字的赛后评价。"
            "必须包含一个具体优点和一个下一步建议，只能使用输入中存在的事实，不得虚构设备状态、动作或成绩。"
        )
    system += "不要使用Markdown、标题、列表或客套开场，长度控制在80到120个汉字左右，最多160字。"
    user = json.dumps(
        {
            "locale": payload.get("locale", "zh-CN"),
            "target_length": payload.get("target_length", "about_100_zh_chars"),
            "game": game,
        },
        ensure_ascii=False,
        separators=(",", ":"),
    )
    return [{"role": "system", "content": system}, {"role": "user", "content": user}]


def generate_feedback(
    payload: Any,
    env: Mapping[str, str] | None = None,
    opener: Callable[..., Any] = urlopen,
) -> str:
    payload = validate_payload(payload)
    settings = env or os.environ
    api_key = settings.get("LLM_API_KEY", "").strip()
    base_url = settings.get("LLM_BASE_URL", "https://api.openai.com/v1").rstrip("/")
    model = settings.get("LLM_MODEL", "").strip()
    path = settings.get("LLM_CHAT_PATH", "/chat/completions").strip("/")
    if not api_key or not model:
        raise FeedbackProxyError("LLM_API_KEY and LLM_MODEL are required")
    try:
        timeout = float(settings.get("LLM_TIMEOUT_SECONDS", "8"))
    except ValueError as exc:
        raise FeedbackProxyError("LLM_TIMEOUT_SECONDS must be numeric") from exc

    request_body = json.dumps(
        {
            "model": model,
            "temperature": 0.7,
            "max_tokens": 180,
            "messages": build_messages(payload),
        },
        ensure_ascii=False,
    ).encode("utf-8")
    request = Request(
        f"{base_url}/{path}",
        data=request_body,
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
            "Accept": "application/json",
        },
        method="POST",
    )
    try:
        with opener(request, timeout=timeout) as response:
            status = getattr(response, "status", None)
            if status is None:
                status = response.getcode()
            body = response.read().decode("utf-8")
    except Exception as exc:  # noqa: BLE001 - proxy returns a safe generic error.
        raise FeedbackProxyError("LLM upstream request failed") from exc
    if status < 200 or status >= 300:
        raise FeedbackProxyError(f"LLM upstream returned HTTP {status}")
    try:
        content = json.loads(body)["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError, json.JSONDecodeError) as exc:
        raise FeedbackProxyError("LLM upstream returned an invalid response") from exc
    feedback = normalize_feedback(content)
    if feedback is None:
        raise FeedbackProxyError("LLM upstream returned invalid feedback length")
    return feedback


class FeedbackHandler(BaseHTTPRequestHandler):
    server_version = "NiulaiFeedbackProxy/0.1"

    def do_POST(self) -> None:  # noqa: N802 - required by BaseHTTPRequestHandler.
        if self.path != "/v1/gameplay-feedback":
            self._send_json(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_REQUEST_BYTES:
                raise FeedbackPayloadError("invalid request size")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            feedback = generate_feedback(payload)
        except (FeedbackProxyError, ValueError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            status = 400 if isinstance(
                exc,
                (FeedbackPayloadError, ValueError, UnicodeDecodeError, json.JSONDecodeError),
            ) else 502
            self._send_json(status, {"error": str(exc)})
            return
        self._send_json(200, {"feedback": feedback})

    def log_message(self, format: str, *args: Any) -> None:
        return

    def _send_json(self, status: int, body: dict[str, str]) -> None:
        encoded = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)


def main() -> None:
    host = os.environ.get("FEEDBACK_PROXY_HOST", DEFAULT_HOST)
    port = int(os.environ.get("FEEDBACK_PROXY_PORT", str(DEFAULT_PORT)))
    with ThreadingHTTPServer((host, port), FeedbackHandler) as server:
        print(f"Niulai feedback proxy listening on {host}:{port}")
        server.serve_forever()


if __name__ == "__main__":
    main()
