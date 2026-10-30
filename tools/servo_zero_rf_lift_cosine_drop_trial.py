#!/usr/bin/env python3
"""Run the isolated RF lift/cosine-swing/cutoff/drop hardware trial."""

from __future__ import annotations

import argparse
import sys
import time

import serial

SUPPORT_FRAME = "1494,1165,1672,1748,1368,1279,1764,1934"


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
    parser.add_argument(
        "--cutoff-after-start-ms",
        type=int,
        default=700,
        help="time after demo start at which to send release (300..1600 ms)",
    )
    parser.add_argument(
        "--swing-degrees",
        type=float,
        default=45.0,
        help="nominal CH0 swing amplitude (0..60 degrees)",
    )
    parser.add_argument(
        "--prepare-support",
        action="store_true",
        help="enter the recorded four-leg support pose before the single-leg demo",
    )
    parser.add_argument("--drop-observe-seconds", type=float, default=0.8)
    args = parser.parse_args()
    if not 300 <= args.cutoff_after_start_ms <= 1600:
        parser.error("--cutoff-after-start-ms must be 300..1600")
    if not 0.0 < args.swing_degrees <= 60.0:
        parser.error("--swing-degrees must be >0..60")

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
        if args.prepare_support:
            send(port, "pose pseudo 100")
            prepared = read_for(port, 3.5)
            if "rest pose reached; holding target; /OE LOW" not in prepared:
                send(port, "stop")
                read_for(port, 1.0)
                print("support pose did not reach its target", file=sys.stderr)
                return 1
            send(port, "stop")
            read_for(port, 0.5)
            send(port, f"frame {SUPPORT_FRAME}")
            framed = read_for(port, 0.4)
            if f"frame -> {SUPPORT_FRAME.replace(',', '/') } us" not in framed:
                send(port, "stop")
                read_for(port, 1.0)
                print("support frame preload failed", file=sys.stderr)
                return 1
        send(port, "stats reset")
        read_for(port, 0.4)
        send(port, f"demo rf-lift-cosine-drop {args.swing_degrees:g}")
        started = read_for(port, args.cutoff_after_start_ms / 1000.0)
        send(port, "release")
        released = read_for(port, args.drop_observe_seconds)
        send(port, "stop")
        stopped = read_for(port, 1.0)
        send(port, "stats")
        stats = read_for(port, 0.8)

    combined = started + released + stopped
    if "RF lift/cosine/drop started" not in combined:
        print("RF lift/cosine/drop demo did not start", file=sys.stderr)
        return 1
    if "motionleg release at" not in combined:
        print("RF lift/cosine/drop demo did not receive release", file=sys.stderr)
        return 1
    if "motionleg touchdown complete" not in combined:
        print("RF lift/cosine/drop demo did not complete touchdown", file=sys.stderr)
        return 1
    if "UART stop -> /OE HIGH" not in stopped:
        print("RF lift/cosine/drop demo did not confirm stop", file=sys.stderr)
        return 1
    if "I2C stats:" not in stats:
        print("RF lift/cosine/drop demo did not return stats", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
