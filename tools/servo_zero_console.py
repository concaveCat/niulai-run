#!/usr/bin/env python3
"""ASCII keyboard console for the isolated PCA9685 servo-zero firmware."""

from __future__ import annotations

import argparse
import os
import re
import select
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - only on an unprepared host
    serial = None


CHANNELS = (
    "CH0 rf_hip  右前髋",
    "CH1 rf_knee 右前膝",
    "CH2 lf_hip  左前髋",
    "CH3 lf_knee 左前膝",
    "CH4 rr_hip  右后髋",
    "CH5 rr_knee 右后膝",
    "CH6 lr_hip  左后髋",
    "CH7 lr_knee 左后膝",
)
CHANNEL_COUNT = len(CHANNELS)
# Deliberately broad test range for measuring the actual joint stop.  This is
# a software transport range, not a claim that every connected servo is safe
# throughout it; the operator must approach each stop manually.
MIN_PULSE_US = 300
MAX_PULSE_US = 3000
DEFAULT_PULSE_US = 1500
PWM_FREQUENCY_HZ = 50
PCA9685_RESOLUTION = 4096
FINE_STEP_US = 1
COARSE_STEP_US = 10
def pulse_to_ticks(pulse_us: int, frequency_hz: int = PWM_FREQUENCY_HZ) -> int:
    """Match the integer conversion used by the ESP32 firmware."""

    return (pulse_us * PCA9685_RESOLUTION * frequency_hz) // 1_000_000


def adjust_pulse(pulse_us: int, delta_us: int) -> int:
    """Adjust the displayed/transmitted pulse width in integer microseconds."""

    return max(MIN_PULSE_US, min(MAX_PULSE_US, pulse_us + delta_us))


def _channel_from_list_line(line: str) -> tuple[int, int] | None:
    match = re.search(r"CH(\d+)\s+[^=]+\s+=\s+(\d+)\s+us", line)
    if match is None:
        return None
    channel, pulse = int(match.group(1)), int(match.group(2))
    if 0 <= channel < CHANNEL_COUNT and MIN_PULSE_US <= pulse <= MAX_PULSE_US:
        return channel, pulse
    return None


class RawTerminal:
    def __enter__(self):
        self._old = None
        if os.name != "nt":
            import termios
            import tty

            self._old = termios.tcgetattr(sys.stdin.fileno())
            tty.setcbreak(sys.stdin.fileno())
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self._old is not None:
            import termios

            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self._old)


def read_posix_key(fd: int) -> str:
    """Read one POSIX terminal key without TextIO buffering."""

    data = os.read(fd, 1)
    if not data:
        return ""
    key = data.decode("latin-1")
    if key != "\x1b":
        return {"\n": "ENTER", "\r": "ENTER"}.get(key, key)
    sequence = ""
    deadline = time.monotonic() + 0.2
    while len(sequence) < 8:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
            break
        data = os.read(fd, 1)
        if not data:
            break
        sequence += data.decode("latin-1")
        if sequence.endswith(("A", "B", "C", "D")):
            break
    return decode_posix_escape(sequence)


def read_key() -> str:
    if os.name == "nt":
        import msvcrt

        key = msvcrt.getwch()
        if key in ("\x00", "\xe0"):
            return {
                "H": "UP",
                "P": "DOWN",
                "K": "LEFT",
                "M": "RIGHT",
            }.get(msvcrt.getwch(), "")
        return {"\r": "ENTER", "\x1b": "ESC"}.get(key, key)

    return read_posix_key(sys.stdin.fileno())


def decode_posix_escape(sequence: str) -> str:
    """Decode common terminal arrow sequences without treating bad input as Esc."""

    if sequence == "":
        return "ESC"
    return {
        "[A": "UP",
        "[B": "DOWN",
        "[D": "LEFT",
        "[C": "RIGHT",
        "OA": "UP",
        "OB": "DOWN",
        "OD": "LEFT",
        "OC": "RIGHT",
    }.get(sequence, "")


class ServoZeroConsole:
    def __init__(self, port: str, baud: int, boot_wait: float):
        if serial is None:
            raise RuntimeError("缺少 pyserial；请先安装 python3-serial 或 pyserial")
        self.ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=0.05,
            write_timeout=2,
            rtscts=False,
            dsrdtr=False,
        )
        # Opening CH340 can reset the ESP32.  Wait for its safe boot sequence.
        self.ser.dtr = False
        self.ser.rts = False
        time.sleep(boot_wait)
        self.pulses = [DEFAULT_PULSE_US] * CHANNEL_COUNT
        self.confirmed = [False] * CHANNEL_COUNT
        self.selected = 0
        self.armed = False
        self.messages: list[str] = []
        self._send("stop")
        self._send("list")
        time.sleep(0.25)
        for line in self._drain():
            parsed = _channel_from_list_line(line)
            if parsed is not None:
                channel, pulse = parsed
                self.pulses[channel] = pulse
        self._send("stop")

    def _drain(self) -> list[str]:
        lines = []
        while self.ser.in_waiting:
            raw = self.ser.readline()
            if raw:
                line = raw.decode("utf-8", "replace").strip()
                if line:
                    lines.append(line)
        self.messages.extend(lines[-8:])
        self.messages = self.messages[-8:]
        return lines

    def _send(self, command: str):
        self.ser.write((command + "\r\n").encode("ascii"))
        self.ser.flush()

    def send(self, command: str):
        self._send(command)
        time.sleep(0.02)
        self._drain()

    def select_channel(self, channel: int):
        self.selected = channel
        self.send(f"select {channel}")

    def adjust(self, delta_us: int):
        pulse = adjust_pulse(self.pulses[self.selected], delta_us)
        if pulse == self.pulses[self.selected]:
            return
        self.pulses[self.selected] = pulse
        self.send(f"pulse {pulse}")

    def center_selected(self):
        self.pulses[self.selected] = DEFAULT_PULSE_US
        self.send("pulse 1500")

    def center_all(self):
        self.pulses = [DEFAULT_PULSE_US] * CHANNEL_COUNT
        self.send("all 1500")

    def confirm_selected(self):
        self.confirmed[self.selected] = True
        self.send("save")

    def toggle_run(self):
        if self.armed:
            self.stop()
        else:
            self.send("run")
            self.armed = True

    def stop(self):
        self.send("stop")
        self.armed = False

    def handle(self, key: str) -> bool:
        if key in ("q", "Q", "ESC"):
            return False
        if key in ("UP", "k", "K"):
            self.select_channel((self.selected - 1) % CHANNEL_COUNT)
        elif key in ("DOWN", "j", "J"):
            self.select_channel((self.selected + 1) % CHANNEL_COUNT)
        elif key == "LEFT":
            self.adjust(-FINE_STEP_US)
        elif key == "RIGHT":
            self.adjust(FINE_STEP_US)
        elif key in ("[", "{"):
            self.adjust(-COARSE_STEP_US)
        elif key in ("]", "}"):
            self.adjust(COARSE_STEP_US)
        elif key.isdigit() and 0 <= int(key) < CHANNEL_COUNT:
            self.select_channel(int(key))
        elif key in ("ENTER", "c", "C"):
            self.confirm_selected() if key == "ENTER" else self.center_selected()
        elif key == "A":
            self.center_all()
        elif key in ("r", "R"):
            self.toggle_run()
        elif key in ("x", "X", " "):
            self.stop()
        return True

    def draw(self):
        self._drain()
        state = "ARMED /OE LOW" if self.armed else "SAFE /OE HIGH"
        selected = self.pulses[self.selected]
        tick = pulse_to_ticks(selected)
        output = ["\x1b[2J\x1b[H", "牛来快跑 Servo Zero Console", "=" * 72]
        output.append(f"状态: {state}    当前: CH{self.selected}    脉宽: {selected} us ({tick} ticks)")
        output.append("左/右: 微调 ±1 us   [ / ]: 粗调 ±10 us   (PCA9685 实际约5 us/计数)")
        output.append("上/下或 J/K: 选择通道   0..7: 直选   Enter: 确认当前值并保存")
        output.append(f"测试范围: {MIN_PULSE_US}..{MAX_PULSE_US} us   R: 启用/停止输出   X/空格: 停止   Q/Esc: 退出")
        output.append("C: 当前通道回1500   A: 全部回1500")
        output.append("")
        for index, name in enumerate(CHANNELS):
            marker = ">" if index == self.selected else " "
            saved = "CONFIRMED" if self.confirmed[index] else "pending"
            output.append(f"{marker} {name:<25} {self.pulses[index]:4d} us  {saved}")
        output.append("")
        output.append("最近设备回显:")
        output.extend(f"  {line[-100:]}" for line in self.messages[-4:])
        sys.stdout.write("\n".join(output) + "\n")
        sys.stdout.flush()

    def close(self):
        try:
            self.stop()
        except Exception:
            pass
        self.ser.close()


class QuadGaitConsole(ServoZeroConsole):
    """Manual four-leg ASCII gait: 1..4 performs one selected-leg step."""

    def __init__(self, port: str, baud: int, boot_wait: float, profile: str):
        super().__init__(port, baud, boot_wait)
        self.profile = profile
        self.armed = True
        self.send("frame 1494,1165,1672,1748,1368,1279,1764,1934")
        self.send("run")
        time.sleep(0.25)

    def handle(self, key: str) -> bool:
        if key in ("q", "Q", "ESC"):
            return False
        if key in ("x", "X", " "):
            self.stop()
        elif key in ("1", "2", "3", "4"):
            self.send(f"demo quad {key} {self.profile}")
        return True

    def draw(self):
        self._drain()
        output = ["\x1b[2J\x1b[H", "牛来快跑 ASCII 手动走行", "=" * 60]
        output.append(f"参数: step,stance,lift,lift_ms,swing_ms,search_ms = {self.profile}")
        output.append("按 1=RF  2=LF  3=RR  4=LR：抬腿→前移→落脚")
        output.append("X/空格: 急停并关闭输出   Q/Esc: 退出并急停")
        output.append("")
        output.extend(f"  {line[-110:]}" for line in self.messages[-6:])
        sys.stdout.write("\n".join(output) + "\n")
        sys.stdout.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--boot-wait", type=float, default=1.5)
    parser.add_argument("--gait", action="store_true",
                        help="数字 1..4 执行一次对应腿的抬腿/前移/落脚")
    parser.add_argument(
        "--profile", default="20,6,12,120,300,120",
        help="走行参数 step,stance,lift,lift_ms,swing_ms,search_ms",
    )
    args = parser.parse_args()

    try:
        console_class = QuadGaitConsole if args.gait else ServoZeroConsole
        if args.gait:
            console = console_class(args.port, args.baud, args.boot_wait,
                                    args.profile)
        else:
            console = console_class(args.port, args.baud, args.boot_wait)
    except Exception as error:
        print(f"无法打开舵机串口: {error}", file=sys.stderr)
        return 2

    try:
        with RawTerminal():
            console.draw()
            while True:
                if not console.handle(read_key()):
                    break
                console.draw()
    except KeyboardInterrupt:
        pass
    finally:
        console.close()
        print("\n已发送 stop，舵机输出关闭。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
