#!/usr/bin/env python3
"""Run the bounded servo-zero benchmark over the ESP32 USB serial console."""

from __future__ import annotations

import argparse
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - depends on the host environment
    serial = None


def write_record(output, text: str) -> None:
    print(text, end="", flush=True)
    if output is not None:
        output.write(text)
        output.flush()


def send_command(port, command: str) -> None:
    port.write((command.strip() + "\r\n").encode("ascii"))
    port.flush()


def receive_until(port, marker: str, timeout: float, output) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace")
        write_record(output, text)
        if marker in text:
            return True
    write_record(sys.stderr, f"[timeout waiting for {marker}]\n")
    return False


def drain_available(port, output) -> str:
    chunks = []
    while port.in_waiting:
        raw = port.read(port.in_waiting)
        if not raw:
            break
        text = raw.decode("utf-8", errors="replace")
        chunks.append(text)
        write_record(output, text)
    return "".join(chunks)


def run_benchmark(args: argparse.Namespace) -> int:
    if serial is None:
        print("connection failed: pyserial is not installed", file=sys.stderr)
        return 2

    output = open(args.output, "a", encoding="utf-8") if args.output else None
    port = None
    try:
        port = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            timeout=0.1,
            write_timeout=2,
            rtscts=False,
            dsrdtr=False,
        )
        port.dtr = False
        port.rts = False
        time.sleep(args.boot_wait)

        write_record(output, "[BENCHMARK] USB serial connection ready\n")
        boot_text = drain_available(port, output)
        if "I2C scan found no ACK" in boot_text or "initialization failed" in boot_text:
            write_record(
                output,
                "[BENCHMARK] boot failed: PCA9685 did not ACK; no motion sent\n",
            )
            return 1
        send_command(port, "stop")
        if not receive_until(port, "UART stop -> /OE HIGH", 3.0, output):
            return 1
        send_command(port, "stats reset")
        if not receive_until(port, "I2C stats reset", 3.0, output):
            return 1

        command = f"pose pseudo {args.amplitude}"
        for round_number in range(1, args.rounds + 1):
            write_record(
                output,
                f"\n[BENCHMARK] round {round_number}/{args.rounds}: {command}\n",
            )
            send_command(port, command)
            if not receive_until(
                port,
                "rest pose reached; holding target; /OE LOW",
                args.timeout,
                output,
            ):
                send_command(port, "stop")
                receive_until(port, "UART stop -> /OE HIGH", 3.0, output)
                return 1
            send_command(port, "stop")
            if not receive_until(port, "UART stop -> /OE HIGH", 3.0, output):
                return 1

        send_command(port, "stats")
        if not receive_until(port, "I2C stats:", 3.0, output):
            return 1
        write_record(output, "\n[BENCHMARK] completed\n")
        return 0
    except (OSError, serial.SerialException) as error:
        print(f"connection failed: {error}", file=sys.stderr)
        return 2
    finally:
        if port is not None:
            try:
                send_command(port, "stop")
                time.sleep(0.1)
            except (OSError, serial.SerialException):
                pass
            port.close()
        if output is not None:
            output.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--boot-wait", type=float, default=2.0)
    parser.add_argument("--rounds", type=int, default=50)
    parser.add_argument("--amplitude", type=int, default=35)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.rounds <= 0 or not 0 <= args.amplitude <= 100 or args.timeout <= 0:
        parser.error("rounds must be >0, amplitude must be 0..100, timeout must be >0")
    return run_benchmark(args)


if __name__ == "__main__":
    raise SystemExit(main())
