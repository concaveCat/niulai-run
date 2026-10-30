#!/usr/bin/env python3
"""Run a bounded ASCII four-leg keyboard-demo regression over USB serial."""

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


def wait_for_any(port, markers: tuple[str, ...], timeout: float, output) -> str | None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace")
        write_record(output, text)
        for marker in markers:
            if marker in text:
                return marker
    write_record(sys.stderr, f"[timeout waiting for one of: {markers}]\n")
    return None


def run_trial(args: argparse.Namespace) -> int:
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

        write_record(output, "[QUAD_TRIAL] USB serial connection ready\n")
        send_command(port, "stop")
        if wait_for_any(port, ("UART stop -> /OE HIGH",), 3.0, output) is None:
            return 1

        send_command(port, "frame 1494,1165,1672,1748,1368,1279,1764,1934")
        send_command(port, "run")
        if wait_for_any(
            port,
            ("run -> /OE LOW", "frame failed:"),
            args.timeout,
            output,
        ) != "run -> /OE LOW":
            send_command(port, "stop")
            wait_for_any(port, ("UART stop -> /OE HIGH",), 3.0, output)
            return 1

        sequence = tuple(str(key) for key in (1, 2, 3, 4))
        sequence *= args.cycles
        for index, key in enumerate(sequence, start=1):
            command = f"demo quad {key}"
            if args.profile:
                command += f" {args.profile}"
            write_record(
                output,
                f"\n[QUAD_TRIAL] step {index}/{len(sequence)}: key={key}\n",
            )
            send_command(port, command)
            accepted = wait_for_any(
                port,
                (
                    f"quad demo key={key} accepted",
                    "demo quad rejected",
                    "quad demo rejected",
                    "quad demo stopped",
                ),
                3.0,
                output,
            )
            if accepted != f"quad demo key={key} accepted":
                send_command(port, "stop")
                wait_for_any(port, ("UART stop -> /OE HIGH",), 3.0, output)
                return 1
            completed = wait_for_any(
                port,
                ("quad demo complete", "quad demo stopped"),
                args.timeout,
                output,
            )
            if completed != "quad demo complete":
                send_command(port, "stop")
                wait_for_any(port, ("UART stop -> /OE HIGH",), 3.0, output)
                return 1

        send_command(port, "stop")
        if wait_for_any(port, ("UART stop -> /OE HIGH",), 3.0, output) is None:
            return 1
        send_command(port, "stats")
        if wait_for_any(port, ("I2C stats:",), 3.0, output) is None:
            return 1
        write_record(output, "\n[QUAD_TRIAL] completed\n")
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
    parser.add_argument("--cycles", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=6.0)
    parser.add_argument(
        "--profile",
        help="optional UART profile: stance_mm,lift_ms,swing_ms,search_ms",
    )
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.cycles <= 0 or args.timeout <= 0:
        parser.error("cycles and timeout must be >0")
    return run_trial(args)


if __name__ == "__main__":
    raise SystemExit(main())
