#!/usr/bin/env python3
"""Run and capture the isolated CH1 cosine-sweep hardware trial."""

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
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--observe-seconds", type=float, default=10.0)
    args = parser.parse_args()

    with serial.Serial(
        args.port,
        args.baud,
        timeout=0.1,
        write_timeout=2,
        rtscts=False,
        dsrdtr=False,
    ) as port:
        port.dtr = False
        port.rts = False
        time.sleep(0.5)
        read_for(port, 0.5)
        send(port, "list")
        read_for(port, 0.8)
        send(port, "stats reset")
        read_for(port, 0.4)
        send(port, "demo ch1-cosine")
        output = read_for(port, args.observe_seconds)
        send(port, "stop")
        final = read_for(port, 1.5)
        send(port, "stats")
        stats = read_for(port, 0.8)

    if "CH1 cosine sweep started" not in output:
        print("CH1 cosine sweep did not start", file=sys.stderr)
        return 1
    if "UART stop -> /OE HIGH" not in final:
        print("CH1 cosine sweep did not confirm stop", file=sys.stderr)
        return 1
    if "I2C stats:" not in stats:
        print("CH1 cosine sweep did not return stats", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
