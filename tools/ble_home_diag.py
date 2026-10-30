#!/usr/bin/env python3
"""Bounded BLE HOME diagnostic: neutral CONTROL_STATE, then HOME only."""

import argparse
import asyncio
import os
import sys
import threading

import serial
from bleak import BleakClient

CONTROL = "7d4e1001-8a52-4d9e-b0f8-57e6c59e2e11"
HOME = "7d4e1003-8a52-4d9e-b0f8-57e6c59e2e11"


def serial_reader(stop: threading.Event, port_name: str) -> None:
    port = serial.Serial()
    port.port = port_name
    port.baudrate = 115200
    port.timeout = 0.2
    port.rtscts = False
    port.dsrdtr = False
    port.dtr = False
    port.rts = False
    port.open()
    try:
        while not stop.is_set():
            line = port.readline()
            if line and any(
                marker in line
                for marker in (
                    b"BLE_HOME",
                    b"FSM_RESET",
                    b"QW_STATUS",
                    b"SAFETY_GATE",
                    b"stack overflow",
                    b"reboot",
                    b"GAP ",
                )
            ):
                sys.stdout.buffer.write(b"UART " + line)
                sys.stdout.buffer.flush()
    finally:
        port.close()


async def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address", default=os.environ.get("NIULAI_BLE_ADDRESS", ""), help="BLE address (or NIULAI_BLE_ADDRESS)")
    parser.add_argument("--port", default=os.environ.get("NIULAI_SERIAL_PORT", "/dev/ttyACM0"), help="UART port for log capture")
    args = parser.parse_args()
    if not args.address:
        parser.error("--address is required")
    stop = threading.Event()
    reader = threading.Thread(target=serial_reader, args=(stop, args.port), daemon=True)
    reader.start()
    try:
        async with BleakClient(args.address, pair=True, timeout=30.0) as client:
            print(f"BLE_CONNECTED={client.is_connected}", flush=True)
            await client.write_gatt_char(CONTROL, bytes((1, 0, 0, 0)), response=True)
            print("NEUTRAL_WRITE=OK", flush=True)
            await asyncio.sleep(0.1)
            await client.write_gatt_char(HOME, bytes((1, 1)), response=True)
            print("HOME_WRITE=OK", flush=True)
            await asyncio.sleep(1.0)
        return 0
    finally:
        stop.set()
        reader.join(timeout=2.0)


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
