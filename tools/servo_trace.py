#!/usr/bin/env python3
"""Bounded Host <-> ESP32 serial trace for the isolated PCA9685 test.

The sender deliberately never reads a reply or flushes the output queue in
the 20 ms command loop. Device output is collected only after the test has
been halted, so diagnostic logging cannot create a stop-and-wait sender.
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
import time
from dataclasses import dataclass
from pathlib import Path

try:
    import serial
except ImportError:  # pragma: no cover - depends on host setup
    serial = None


MIN_PULSE_US = 500
MAX_PULSE_US = 2500
DEFAULT_PERIOD_S = 0.020


@dataclass(frozen=True)
class HostTx:
    sequence: int
    timestamp_ns: int
    pulse_us: int


@dataclass(frozen=True)
class DeviceRx:
    sequence: int
    timestamp_us: int
    pulse_us: int
    interval_us: int


_ESP_RX_RE = re.compile(
    r"ESP_RX seq=(?P<seq>\d+) timestamp_us=(?P<ts>\d+) "
    r"pulse=(?P<pulse>\d+) rx_interval_us=(?P<interval>-?\d+)"
)
_TICK_RE = re.compile(
    r"CONTROL_TICK id=(?P<id>\d+) timestamp_us=(?P<ts>\d+) "
    r"tick_interval_us=(?P<interval>-?\d+)"
)
_I2C_END_RE = re.compile(
    r"I2C_END seq=(?P<seq>\d+) timestamp_us=(?P<ts>\d+) "
    r"result=0x(?P<result>[0-9a-fA-F]+) duration_us=(?P<duration>-?\d+)"
)


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return ordered[lower] + (ordered[upper] - ordered[lower]) * weight


def interval_stats(intervals_ms: list[float]) -> dict[str, float]:
    if not intervals_ms:
        return {key: 0.0 for key in ("min", "mean", "p95", "p99", "max")}
    return {
        "min": min(intervals_ms),
        "mean": statistics.fmean(intervals_ms),
        "p95": percentile(intervals_ms, 0.95),
        "p99": percentile(intervals_ms, 0.99),
        "max": max(intervals_ms),
    }


def parse_device_lines(lines: list[str]) -> tuple[list[DeviceRx], list[float], list[float]]:
    rx: list[DeviceRx] = []
    ticks_ms: list[float] = []
    i2c_durations_ms: list[float] = []
    for line in lines:
        match = _ESP_RX_RE.search(line)
        if match:
            rx.append(
                DeviceRx(
                    sequence=int(match["seq"]),
                    timestamp_us=int(match["ts"]),
                    pulse_us=int(match["pulse"]),
                    interval_us=int(match["interval"]),
                )
            )
        match = _TICK_RE.search(line)
        if match:
            ticks_ms.append(int(match["interval"]) / 1000.0)
        match = _I2C_END_RE.search(line)
        if match:
            i2c_durations_ms.append(int(match["duration"]) / 1000.0)
    return rx, ticks_ms, i2c_durations_ms


def summarize_device_lines(lines: list[str]) -> str:
    rx, ticks_ms, i2c_durations_ms = parse_device_lines(lines)
    rx_intervals_ms = [item.interval_us / 1000.0 for item in rx if item.interval_us > 0]
    batch_count = sum(1 for item in rx if 0 < item.interval_us <= 2000)
    long_rx_gaps = sum(1 for item in rx if item.interval_us > 60000)
    output = [
        "ESP RX records: %d" % len(rx),
        "ESP RX interval ms: %s" % format_stats(interval_stats(rx_intervals_ms)),
        "ESP RX <=2 ms batches: %d" % batch_count,
        "ESP RX >60 ms gaps: %d" % long_rx_gaps,
    ]
    if ticks_ms:
        output.append("control/update interval ms: %s" % format_stats(interval_stats(ticks_ms)))
    if i2c_durations_ms:
        output.append("I2C duration ms: %s" % format_stats(interval_stats(i2c_durations_ms)))
    return "\n".join(output)


def format_stats(stats: dict[str, float]) -> str:
    return " ".join("%s=%.3f" % (key, stats[key]) for key in ("min", "mean", "p95", "p99", "max"))


class ServoTraceClient:
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
        self.ser.dtr = False
        self.ser.rts = False
        time.sleep(boot_wait)

    def send(self, command: str, flush: bool = False) -> int:
        timestamp_ns = time.perf_counter_ns()
        self.ser.write((command + "\n").encode("ascii"))
        if flush:
            self.ser.flush()
        return timestamp_ns

    def drain(self) -> list[str]:
        lines: list[str] = []
        while self.ser.in_waiting:
            raw = self.ser.read(self.ser.in_waiting)
            if not raw:
                break
            lines.extend(raw.decode("utf-8", "replace").splitlines())
        return [line for line in lines if line]

    def collect_until_marker(self, marker: str, timeout_s: float) -> list[str]:
        lines: list[str] = []
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            if self.ser.in_waiting:
                raw = self.ser.readline()
                if raw:
                    line = raw.decode("utf-8", "replace").rstrip("\r\n")
                    lines.append(line)
                    if line == marker:
                        return lines
            else:
                time.sleep(0.005)
        raise TimeoutError("did not receive %s before timeout" % marker)

    def close(self) -> None:
        self.ser.close()


def format_host_stats(records: list[HostTx]) -> str:
    intervals_ms = [
        (current.timestamp_ns - previous.timestamp_ns) / 1_000_000.0
        for previous, current in zip(records, records[1:])
    ]
    stats = interval_stats(intervals_ms)
    return "HOST TX interval ms: %s" % format_stats(stats)


def send_periodic_targets(
    client: ServoTraceClient,
    records: list[HostTx],
    count: int,
    period_s: float,
    start_pulse: int,
    step_pulse: int,
) -> None:
    """Send on one absolute schedule; do not accumulate per-item sleeps."""
    next_deadline = time.perf_counter()
    for sequence in range(count):
        while time.perf_counter() < next_deadline:
            time.sleep(min(0.001, max(0.0, next_deadline - time.perf_counter())))
        pulse = start_pulse + (step_pulse if sequence % 2 else 0)
        timestamp_ns = client.send("CMD %d %d" % (sequence, pulse))
        records.append(HostTx(sequence, timestamp_ns, pulse))
        next_deadline += period_s


def run(args: argparse.Namespace) -> int:
    if args.mode == "e2e" and not args.allow_output and not args.dry_run:
        print("e2e 会拉低 /OE；必须显式提供 --allow-output，并先确认单舵机安全条件", file=sys.stderr)
        return 2
    log_path = Path(args.log)
    records: list[HostTx] = []
    client: ServoTraceClient | None = None
    raw_lines: list[str] = []
    try:
        client = ServoTraceClient(args.port, args.baud, args.boot_wait)
        raw_lines.extend("HOST_RX timestamp_ns=%d raw=%s" % (time.perf_counter_ns(), line) for line in client.drain())
        client.send("stop", flush=True)
        client.send("trace begin %s" % args.mode, flush=True)
        time.sleep(0.05)
        raw_lines.extend("HOST_RX timestamp_ns=%d raw=%s" % (time.perf_counter_ns(), line) for line in client.drain())

        if args.mode == "rx":
            send_periodic_targets(
                client, records, args.count, args.period_s,
                args.start_pulse, args.step_pulse,
            )
        elif args.mode == "e2e":
            client.send("select %d" % args.channel, flush=True)
            if args.legacy:
                run_command = "trace run-legacy-dry" if args.dry_run else "trace run-legacy"
            else:
                run_command = "trace run-dry" if args.dry_run else "trace run"
            client.send(run_command, flush=True)
            send_periodic_targets(
                client, records, args.count, args.period_s,
                args.start_pulse, args.step_pulse,
            )
            time.sleep(max(0.1, args.period_s * 4))
        else:
            client.send(args.local_command, flush=True)
            time.sleep(args.duration_s)

        client.send("trace halt", flush=True)
        time.sleep(0.05)
        client.send("trace dump", flush=True)
        summary_lines = client.collect_until_marker("TRACE_DUMP_END", args.dump_timeout_s)
        raw_lines.extend(
            "HOST_RX timestamp_ns=%d raw=%s" % (time.perf_counter_ns(), line)
            for line in summary_lines
        )
        summary = next((line for line in summary_lines if "TRACE_DUMP_BEGIN" in line), "")
        match = re.search(r"records=(\d+)", summary)
        total_records = int(match.group(1)) if match else 0
        for offset in range(0, total_records, 32):
            client.send("trace page %d 32" % offset, flush=True)
            page_lines = client.collect_until_marker("TRACE_PAGE_END", args.dump_timeout_s)
            raw_lines.extend(
                "HOST_RX timestamp_ns=%d raw=%s" % (time.perf_counter_ns(), line)
                for line in page_lines
            )
    except (OSError, RuntimeError, TimeoutError) as error:
        print("trace failed: %s" % error, file=sys.stderr)
        if client is not None:
            try:
                client.send("stop", flush=True)
            except Exception:
                pass
        return 2
    finally:
        if client is not None:
            client.close()

    log_path.write_text("\n".join("HOST_TX seq=%d timestamp_ns=%d pulse=%d" % (r.sequence, r.timestamp_ns, r.pulse_us) for r in records) + "\n" + "\n".join(raw_lines) + "\n", encoding="utf-8")
    print("log: %s" % log_path)
    print(format_host_stats(records))
    print(summarize_device_lines(raw_lines))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--boot-wait", type=float, default=1.5)
    parser.add_argument("--mode", choices=("rx", "local", "e2e"), default="rx")
    parser.add_argument("--count", type=int, default=500)
    parser.add_argument("--period-ms", type=float, default=20.0)
    parser.add_argument("--start-pulse", type=int, default=1500)
    parser.add_argument("--step-pulse", type=int, default=5)
    parser.add_argument("--channel", type=int, default=0)
    parser.add_argument("--duration-s", type=float, default=3.5)
    parser.add_argument("--local-command", default="pose pseudo 25")
    parser.add_argument("--dump-timeout-s", type=float, default=15.0)
    parser.add_argument("--allow-output", action="store_true")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="e2e only: send PCA frames while keeping /OE HIGH",
    )
    parser.add_argument(
        "--legacy",
        action="store_true",
        help="e2e only: use the old CONFIG_SERVO_ZERO_REFRESH_MS writer",
    )
    parser.add_argument(
        "--log",
        default="/tmp/niulai-servo-trace-%d.log" % time.time_ns(),
    )
    args = parser.parse_args()
    args.period_s = args.period_ms / 1000.0
    if args.period_s <= 0 or args.count < 1:
        parser.error("--period-ms and --count must be positive")
    if args.legacy and args.mode != "e2e":
        parser.error("--legacy is only valid with --mode e2e")
    if not MIN_PULSE_US <= args.start_pulse <= MAX_PULSE_US:
        parser.error("--start-pulse is outside the software-safe transport range")
    if not MIN_PULSE_US <= args.start_pulse + args.step_pulse <= MAX_PULSE_US:
        parser.error("--step-pulse would leave the software-safe transport range")
    return args


if __name__ == "__main__":
    raise SystemExit(run(build_parser()))
