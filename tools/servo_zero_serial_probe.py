#!/usr/bin/env python3
"""Read isolated servo-zero boot diagnostics without sending commands."""

from __future__ import annotations

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - depends on the host environment
    serial = None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--boot-wait", type=float, default=2.0)
    parser.add_argument("--read-seconds", type=float, default=5.0)
    args = parser.parse_args()
    if serial is None:
        print("probe failed: pyserial is not installed", file=sys.stderr)
        return 2

    try:
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
            time.sleep(args.boot_wait)
            deadline = time.monotonic() + args.read_seconds
            received = bytearray()
            while time.monotonic() < deadline:
                raw = port.read(port.in_waiting or 1)
                if raw:
                    received.extend(raw)
                    sys.stdout.write(raw.decode("utf-8", errors="replace"))
                    sys.stdout.flush()
            text = received.decode("utf-8", errors="replace")
            configured = "I2C device ACK at 0x40 (configured PCA9685)" in text
            addresses = re.findall(r"I2C device ACK at (0x[0-9a-fA-F]{2})", text)
            if configured:
                print("\nPROBE_ACK=CONFIGURED_0x40")
                return 0
            if addresses:
                unique_addresses = ",".join(dict.fromkeys(addresses))
                print(f"\nPROBE_ACK=UNEXPECTED_{unique_addresses}", file=sys.stderr)
            else:
                print("\nPROBE_ACK=NONE", file=sys.stderr)
            return 1
    except (OSError, serial.SerialException) as error:
        print(f"probe failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
