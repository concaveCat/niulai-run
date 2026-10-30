#!/usr/bin/env python3
"""Minimal ASCII four-leg walker for the isolated servo-zero firmware.

 State machine: SAFE -> REST_READY -> stepping.  Digits are ignored until S
 has successfully loaded the confirmed rest snapshot.  Each digit owns one
 leg; key-down and key-up are sent independently, while the ESP32 calculates
 support-leg retreat for the complete frame.
"""

from __future__ import annotations

import argparse
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover
    serial = None


REST_READY = "direct rest ready"
STOPPED = "UART stop -> /OE HIGH"
PRESS_ACCEPTED = "accepted"
PRESS_DEFERRED = "deferred"
PRESS_REJECTED = "rejected"
PROFILE_30MM = "30,9,30,300,360,120,25"
PROFILE_40MM = "30,9,40,300,360,120,25"
PROFILE_LARGE_SWING_40MM = "30,9,40,300,360,120,50"
PROFILE_HALF_HIP_40MM = "30,9,40,300,360,120,50"
PROFILE_KNEE_DOMINANT_40MM = PROFILE_LARGE_SWING_40MM
DEFAULT_PROFILE = PROFILE_LARGE_SWING_40MM
PRESS_RETRY_TIMEOUT_S = 1.5


def full_swing_hold_seconds(profile: str) -> float:
    """Return a sequence-test hold long enough to reach the swing target.

    The interactive path is governed by real key-up events.  ``--sequence``
    is an automated regression path, so it should exercise the configured
    lift plus full swing instead of releasing halfway through the swing.
    Invalid profiles are left for firmware validation and retain the old
    conservative delay.
    """
    try:
        values = [int(value) for value in profile.split(",")]
    except ValueError:
        return 0.5
    if len(values) == 4:
        lift_ms, swing_ms = values[1], values[2]
    elif len(values) in (6, 7):
        lift_ms, swing_ms = values[3], values[4]
    else:
        return 0.5
    if lift_ms < 0 or swing_ms < 0:
        return 0.5
    return max(0.5, (lift_ms + swing_ms + 40) / 1000.0)


def has_terminal_quad_failure(lines: list[str]) -> bool:
    """Return only failures that ended/rejected the logical quad command.

    The firmware retries each PCA9685 transaction.  Its per-attempt warning
    contains ``I2C failed`` even when the next attempt succeeds, so it must
    not be treated as a command failure by the keyboard client.
    """
    terminal_markers = (
        "quad demo stopped",
        "quad demo rejected",
        "direct rest rejected",
    )
    return any(any(marker in line for marker in terminal_markers)
               for line in lines)


def release_completion_observed(lines: list[str]) -> bool:
    """Return true when release already completed or was latched by firmware.

    A key can be released after the fixed trajectory has completed, or while
    its SEARCH landing is still running.  In both cases the firmware reports
    ``already applied`` and will not emit a second completion line later.
    """
    return any("quad demo complete" in line or
               "quad key release already applied" in line
               for line in lines)


def classify_quad_press_response(lines: list[str]) -> str:
    """Classify a press as accepted, retryable, or permanently rejected."""
    if has_terminal_quad_failure(lines):
        return PRESS_REJECTED
    if any(("quad press" in line and "accepted" in line) or
           "quad press started" in line for line in lines):
        return PRESS_ACCEPTED
    if any("quad press rejected" in line or
           "demo quad press/release rejected" in line or
           "profile must be" in line or
           "candidate path exceeds" in line or
           "pulse limit violation" in line for line in lines):
        return PRESS_REJECTED
    # Busy trajectory or a missing UART response is recoverable while the key
    # remains held.  A key-up handler must still forward release for this
    # uncertain state: the command may have reached the ESP32 even though its
    # acceptance line was not observed in the short drain window.
    return PRESS_DEFERRED


class QuadAsciiWalk:
    def __init__(self, port: str, baud: int, boot_wait: float, profile: str):
        if serial is None:
            raise RuntimeError("pyserial is not installed")
        self.port = serial.Serial(
            port, baudrate=baud, timeout=0.05, write_timeout=2,
            rtscts=False, dsrdtr=False,
        )
        self.port.dtr = False
        self.port.rts = False
        time.sleep(boot_wait)
        self.profile = profile
        self.sequence_hold_seconds = full_swing_hold_seconds(profile)
        self.rest_ready = False
        self.running = True
        self._send("stop")
        self._drain(0.2)

    def _send(self, command: str) -> None:
        self.port.write((command + "\r\n").encode("ascii"))
        self.port.flush()

    def _drain(self, seconds: float = 0.0) -> list[str]:
        end = time.monotonic() + seconds
        lines: list[str] = []
        while time.monotonic() < end or self.port.in_waiting:
            raw = self.port.readline()
            if raw:
                line = raw.decode("utf-8", errors="replace").strip()
                if line:
                    lines.append(line)
            elif time.monotonic() >= end:
                break
        for line in lines:
            print(f"[ESP32] {line}")
        return lines

    def enter_rest(self) -> bool:
        self._send("stop")
        self._drain(0.15)
        self._send("rest direct")
        lines = self._drain(0.5)
        self.rest_ready = any(REST_READY in line for line in lines)
        print("[状态] 静息就绪" if self.rest_ready else "[状态] 静息失败")
        return self.rest_ready

    def step(self, key: str) -> bool:
        if not self.rest_ready:
            print("[忽略] 请先按 S 进入静息模式")
            return False
        mask = 1 << (int(key) - 1)
        deadline = time.monotonic() + PRESS_RETRY_TIMEOUT_S
        while True:
            status = self.press_status(mask)
            if status == PRESS_ACCEPTED:
                break
            if status == PRESS_REJECTED or time.monotonic() >= deadline:
                print(f"[拒绝] 腿 {key} 的按下未在有界时间内确认")
                return False
            time.sleep(0.12)
        time.sleep(self.sequence_hold_seconds)
        return self.release(mask)

    def press_status(self, mask: int) -> str:
        if not self.rest_ready:
            print("[忽略] 请先按 S 进入静息模式")
            return PRESS_REJECTED
        stale = self._drain(0.0)
        if has_terminal_quad_failure(stale):
            self.rest_ready = False
            print("[故障] 已检测到异步终止；请按 S 重新进入静息模式")
            return PRESS_REJECTED
        self._send(f"demo quad press {mask} {self.profile}")
        lines = self._drain(0.12)
        if has_terminal_quad_failure(lines):
            self.rest_ready = False
        return classify_quad_press_response(lines)

    def press(self, mask: int) -> bool:
        return self.press_status(mask) == PRESS_ACCEPTED

    def release(self, mask: int, wait_for_complete: bool = True) -> bool:
        # A delayed completion from a previous leg must not satisfy this
        # release.  Firmware state is authoritative; this only removes stale
        # UART text before correlating the next acknowledgement.
        stale = self._drain(0.0)
        if has_terminal_quad_failure(stale):
            self.rest_ready = False
            return False
        self._send(f"demo quad release {mask}")
        lines = self._drain(0.15)
        if has_terminal_quad_failure(lines):
            self.stop()
            return False
        if release_completion_observed(lines):
            return True
        if not wait_for_complete:
            if any("quad key release" in line and
                   ("released" in line or "already" in line)
                   for line in lines):
                return True
            # Release is a one-way user event. Do not leave the host-side
            # held mask stuck merely because its acknowledgement was delayed.
            print(f"[发送] release mask={mask} 已发送，未及时收到回显；继续处理后续按键")
            return True
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            lines = self._drain(0.1)
            if release_completion_observed(lines):
                return True
            if has_terminal_quad_failure(lines):
                self.stop()
                return False
        print("[超时] 未收到 quad demo complete，执行急停")
        self.stop()
        return False

    def stop(self) -> None:
        self._send("stop")
        self._drain(0.3)
        self.rest_ready = False

    def close(self) -> None:
        try:
            self.stop()
        finally:
            self.port.close()


def read_key_event(timeout: float = 0.05) -> tuple[str, str]:
    if sys.platform == "win32":
        import ctypes
        from ctypes import wintypes

        class KeyEvent(ctypes.Structure):
            _fields_ = [("down", wintypes.BOOL), ("repeat", wintypes.WORD),
                        ("vk", wintypes.WORD), ("scan", wintypes.WORD),
                        ("char", wintypes.WCHAR),
                        ("control", wintypes.DWORD)]

        class Record(ctypes.Structure):
            _fields_ = [("event_type", wintypes.WORD), ("key", KeyEvent)]

        kernel = ctypes.windll.kernel32
        handle = kernel.GetStdHandle(-10)  # STD_INPUT_HANDLE
        record = Record()
        read = wintypes.DWORD()
        while True:
            wait_result = kernel.WaitForSingleObject(
                handle, max(0, int(timeout * 1000)))
            if wait_result != 0:
                return "none", ""
            if not kernel.ReadConsoleInputW(handle, ctypes.byref(record), 1,
                                            ctypes.byref(read)):
                return "down", "q"
            if record.event_type == 0x0001 and record.key.char:
                return ("down" if record.key.down else "up",
                        record.key.char)
    import select
    import termios
    import tty
    fd = sys.stdin.fileno()
    if not select.select([fd], [], [], timeout)[0]:
        return "none", ""
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        return "down", sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--boot-wait", type=float, default=1.5)
    parser.add_argument(
        "--profile",
        default=DEFAULT_PROFILE,
        help=("六/七字段 profile；膝主导默认值为 "
              f"{PROFILE_KNEE_DOMINANT_40MM}；30 mm 参考值为 "
              f"{PROFILE_30MM}，40 mm 高净空候选为 {PROFILE_40MM}；"
              f"半髋 40 mm 参考值为 {PROFILE_HALF_HIP_40MM}"),
    )
    parser.add_argument("--sequence", default=None,
                        help="可选：按给定数字序列自动测试，例如 1；完成后自动停止")
    args = parser.parse_args()
    try:
        walker = QuadAsciiWalk(args.port, args.baud, args.boot_wait,
                               args.profile)
    except Exception as error:
        print(f"无法打开串口: {error}", file=sys.stderr)
        return 2

    print("牛来快跑 四腿 ASCII 走行")
    print("S=静息  1=RF  2=LF  3=RR  4=LR  X=急停  Q=退出")
    print(f"profile={args.profile}")
    try:
        if args.sequence is not None:
            if not walker.enter_rest():
                return 1
            for key in args.sequence:
                if key not in "1234" or not walker.step(key):
                    return 1
            walker.stop()
            return 0
        active_mask = 0
        held_mask = 0
        pending_mask = 0
        next_press_at: dict[int, float] = {}
        release_due_at: dict[int, float] = {}

        def service_releases() -> None:
            nonlocal active_mask, pending_mask
            now = time.monotonic()
            for leg in range(4):
                bit = 1 << leg
                due_at = release_due_at.get(bit)
                if due_at is None or now < due_at:
                    continue
                remaining_mask = active_mask & ~bit
                if walker.release(bit, wait_for_complete=remaining_mask == 0):
                    active_mask = remaining_mask
                    release_due_at.pop(bit, None)
                else:
                    active_mask = 0
                    release_due_at.clear()
                    pending_mask = 0
                    next_press_at.clear()
                    return

        def service_pending() -> None:
            nonlocal active_mask, pending_mask, release_due_at
            if not walker.rest_ready:
                pending_mask = 0
                next_press_at.clear()
                return
            now = time.monotonic()
            for leg in range(4):
                bit = 1 << leg
                if (pending_mask & bit) == 0:
                    continue
                if (held_mask & bit) == 0:
                    pending_mask &= ~bit
                    next_press_at.pop(bit, None)
                    continue
                if now < next_press_at.get(bit, 0.0):
                    continue
                status = walker.press_status(bit)
                if status == PRESS_ACCEPTED:
                    active_mask |= bit
                    pending_mask &= ~bit
                    next_press_at.pop(bit, None)
                    print(f"[状态] 排队腿 {leg + 1} 已启动")
                elif status == PRESS_REJECTED:
                    pending_mask &= ~bit
                    next_press_at.pop(bit, None)
                    print(f"[拒绝] 腿 {leg + 1} 的参数或安全检查未通过")
                    if not walker.rest_ready:
                        active_mask = 0
                        pending_mask = 0
                        next_press_at.clear()
                        release_due_at.clear()
                else:
                    next_press_at[bit] = now + 0.12

        while True:
            service_releases()
            service_pending()
            event, key = read_key_event()
            if event == "none":
                continue
            if event == "down" and key in ("q", "Q", "\x1b"):
                break
            if event == "down" and key in ("s", "S"):
                walker.enter_rest()
                active_mask = 0
                pending_mask = 0
                held_mask = 0
                next_press_at.clear()
                release_due_at.clear()
            elif event == "down" and key in ("1", "2", "3", "4"):
                bit = 1 << (int(key) - 1)
                held_mask |= bit
                release_due_at.pop(bit, None)
                if walker.rest_ready and (active_mask & bit) == 0 and \
                        (pending_mask & bit) == 0:
                    status = walker.press_status(bit)
                    if status == PRESS_ACCEPTED:
                        active_mask |= bit
                    elif status == PRESS_DEFERRED:
                        pending_mask |= bit
                        next_press_at[bit] = time.monotonic() + 0.12
                        print(f"[排队] 腿 {int(key)} 正在等待当前动作窗口")
            elif event == "up" and key in ("1", "2", "3", "4"):
                bit = 1 << (int(key) - 1)
                held_mask &= ~bit
                if (pending_mask & bit) != 0:
                    pending_mask &= ~bit
                    next_press_at.pop(bit, None)
                    # Pending means "acceptance not yet correlated", not
                    # "press definitely never ran".  Forward key-up so a
                    # delayed/quiet press cannot leave the knee in LIFT or
                    # the hip in an indefinite hold on the ESP32.
                    if not walker.release(bit, wait_for_complete=False):
                        active_mask = 0
                        pending_mask = 0
                        next_press_at.clear()
                        release_due_at.clear()
                        return 1
                if (active_mask & bit) != 0:
                    # Key-up is forwarded immediately.  The firmware owns
                    # the live swing timer and freezes the hip at this exact
                    # release sample before starting the fixed knee drop.
                    release_due_at[bit] = time.monotonic()
            elif event == "down" and key in ("x", "X", " "):
                walker.stop()
                active_mask = 0
                pending_mask = 0
                held_mask = 0
                next_press_at.clear()
                release_due_at.clear()
    except KeyboardInterrupt:
        pass
    finally:
        walker.close()
        print("已停止，舵机输出关闭。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
