#!/usr/bin/env python3
"""Run and capture one bounded RF vertical-shank sweep trial."""

from __future__ import annotations

import argparse
import sys
import time

import serial


def read_for(port: serial.Serial, seconds: float) -> str:
    deadline = time.monotonic() + seconds
    chunks: list[bytes] = []
    while time.monotonic() < deadline:
        raw = port.read(port.in_waiting or 1)
        if raw:
            chunks.append(raw)
            sys.stdout.buffer.write(raw)
            sys.stdout.buffer.flush()
    return b"".join(chunks).decode("utf-8", errors="replace")


def send(port: serial.Serial, command: str) -> None:
    port.write((command + "\n").encode("ascii"))
    port.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("positive_us", type=int)
    parser.add_argument("negative_us", type=int)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--observe-seconds", type=float, default=5.5)
    args = parser.parse_args()
    if not 500 <= args.positive_us <= 2500:
        parser.error("positive_us must be 500..2500")
    if not 500 <= args.negative_us <= 2500:
        parser.error("negative_us must be 500..2500")
    if args.positive_us <= args.negative_us:
        parser.error("positive_us must be greater than negative_us")
    if args.observe_seconds < 4.5:
        parser.error("observe-seconds must cover the 4000 ms sweep")

    output = ""
    stopped = ""
    stats = ""
    failure: BaseException | None = None
    port: serial.Serial | None = None
    try:
        port = serial.Serial(
            args.port,
            args.baud,
            timeout=0.1,
            write_timeout=2,
            rtscts=False,
            dsrdtr=False,
        )
        port.dtr = False
        port.rts = False
        time.sleep(0.5)
        read_for(port, 0.5)
        send(port, "stop")
        read_for(port, 0.8)
        send(port, "list")
        read_for(port, 0.8)
        send(port, "stats reset")
        read_for(port, 0.4)
        send(
            port,
            f"demo rf-hip-vertical {args.positive_us} {args.negative_us}",
        )
        output = read_for(port, args.observe_seconds)
    except (OSError, serial.SerialException) as error:
        failure = error
    finally:
        if port is not None:
            try:
                send(port, "stop")
                stopped = read_for(port, 1.2)
                send(port, "stats")
                stats = read_for(port, 0.8)
            except (OSError, serial.SerialException) as error:
                if failure is None:
                    failure = error
            port.close()
    if failure is not None:
        print(f"RF vertical-shank trial failed: {failure}", file=sys.stderr)
        return 2

    checks = {
        "start": "HIGH RISK RF vertical-shank sweep started" in output,
        "complete": "vertical sweep complete" in output,
        "stop": "UART stop -> /OE HIGH" in stopped,
        "stats": "I2C stats:" in stats,
    }
    for name, passed in checks.items():
        print(f"TRIAL_{name.upper()}={'PASS' if passed else 'FAIL'}")
    if not checks["start"]:
        print("RF vertical-shank sweep did not start", file=sys.stderr)
    if not checks["complete"]:
        print("RF vertical-shank sweep did not complete before stop", file=sys.stderr)
    if not checks["stop"]:
        print("RF vertical-shank sweep did not confirm stop", file=sys.stderr)
    if not checks["stats"]:
        print("RF vertical-shank sweep did not return stats", file=sys.stderr)
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
