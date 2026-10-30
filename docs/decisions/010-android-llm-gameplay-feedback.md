# Decision: optional LLM gameplay feedback

Status:
- Accepted for the Android Test Ctrl prototype by the requesting Operator on 2026-09-10

Problem:
The Result page should give the player a short, personalized post-game comment of about 100 Chinese characters while remaining useful when the network or model service is unavailable.

Decision:
Use an optional result-page AI feedback card. When enabled, Android sends only aggregate gameplay metrics to a small local/server-side proxy. The proxy calls an OpenAI-compatible `POST /chat/completions` endpoint and returns one plain-text Chinese paragraph. The LLM writes wording only; it does not calculate or alter speed score, coordination score, diagnostics, replay data, BLE output, or hardware behavior.

Privacy and scope:
- Send mode, elapsed time, mock progress/checkpoint, existing scores, event count, per-leg press counts, rhythm consistency, highlight and suggestion.
- Do not send the raw event timeline, device identifiers, BLE data, or credentials.
- Keep the toggle visible on the Result card and enabled by default; persist the player's choice locally.
- Keep the deterministic local Result cards regardless of AI availability.

Fallback and limits:
The app uses a local deterministic paragraph if the proxy fails or returns text outside the 20–160 character validation range. The proxy validates aggregate input, requests roughly 80–120 Chinese characters, caps upstream output at 180 tokens, and does not log request bodies. No real API-key end-to-end test is claimed until an Operator configures a service and performs a manual network test.

Consequences:
- The emulator can use the default host bridge `10.0.2.2:8787`; a physical phone needs a reachable LAN proxy address at build time.
- The feature adds the Android INTERNET permission and a dependency-free Python service, but no Android networking library or API key storage.
- Service availability, model quality, cost, retention and authentication remain deployment concerns; the offline test loop remains fully usable without them.

Follow-up implementation:
The Android home page now exposes a settings screen for API URL, API key, model, and an explicit local-proxy toggle. The default is direct DeepSeek compatibility mode (`https://api.deepseek.com/v1`, model `deepseek-chat`); a configured key requests `/chat/completions`. The proxy toggle requests `/v1/gameplay-feedback` and permits an empty key. The key is encrypted with Android Keystore and shown with password masking. Logcat diagnostics report mode, redacted endpoint, model, HTTP status, and fallback reason without logging the key; the result card keeps only the generated/local feedback text. This is a client-side convenience for prototype testing, not a production secret-management boundary; use short-lived, least-privilege test keys and rotate them after testing.
