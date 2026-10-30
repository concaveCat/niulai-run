#!/usr/bin/env python3
"""Bounded BLE diagnostic: rotate each leg, then exercise two-leg masks."""

import argparse
import asyncio
import os
import sys
import threading
import time

import serial
from bleak import BleakClient

CONTROL = "7d4e1001-8a52-4d9e-b0f8-57e6c59e2e11"
STATUS = "7d4e1002-8a52-4d9e-b0f8-57e6c59e2e11"
TELEMETRY = "7d4e1004-8a52-4d9e-b0f8-57e6c59e2e11"
ROUNDS = 3
SINGLE_LEG_MASKS = (0x01, 0x02, 0x04, 0x08)  # FL, FR, RL, RR
DUAL_LEG_MASKS = (0x09, 0x06, 0x03, 0x0C)  # diagonal, diagonal, adjacent, adjacent


def serial_reader(stop: threading.Event, port_name: str) -> None:
    # Configure modem-control lines before opening the serial port.  Opening a
    # Windows serial port with pyserial defaults can toggle DTR/RTS and reset
    # the ESP32, which would invalidate the BLE/action timing being diagnosed.
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
                    b"control_diag",
                    b"FSM_",
                    b"SAFETY_GATE",
                    b"WATCHDOG",
                    b"stack overflow",
                    b"PCA",
                    b"I2C error",
                    b"GAP ",
                    b"BLE security",
                    b"BLE_GATT",
                    b"reboot",
                )
            ):
                sys.stdout.buffer.write(b"UART " + line)
                sys.stdout.buffer.flush()
    finally:
        port.close()


async def send_packet(client: BleakClient, sequence: int, mask: int) -> None:
    packet = bytes((1, sequence & 0xFF, mask & 0x0F, 0))
    await client.write_gatt_char(CONTROL, packet, response=True)
    if mask != 0 or sequence % 25 == 0:
        print(f"BLE_TX seq={sequence & 0xFF} mask=0x{mask:02X}", flush=True)


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

            def notification(_characteristic: object, _data: bytearray) -> None:
                pass

            await client.start_notify(STATUS, notification)
            await client.start_notify(TELEMETRY, notification)
            print("BLE_NOTIFICATIONS=subscribed", flush=True)
            sequence = 0
            # Keep the watchdog fresh and satisfy the 1-second acceptance
            # window with a margin of neutral input before any nonzero mask is
            # sent.
            for _ in range(15):
                await send_packet(client, sequence, 0)
                sequence += 1
                await asyncio.sleep(0.1)
            for cycle in range(1, ROUNDS + 1):
                print(f"CYCLE_BEGIN={cycle}", flush=True)
                for label, masks in (("SINGLE", SINGLE_LEG_MASKS),
                                     ("DUAL", DUAL_LEG_MASKS)):
                    for index, mask in enumerate(masks, 1):
                        print(f"{label}_BEGIN cycle={cycle} index={index} mask=0x{mask:02X}", flush=True)
                        for _ in range(8):
                            await send_packet(client, sequence, mask)
                            sequence += 1
                            await asyncio.sleep(0.1)
                        for _ in range(8):
                            await send_packet(client, sequence, 0)
                            sequence += 1
                            await asyncio.sleep(0.1)
                        print(f"{label}_END cycle={cycle} index={index} mask=0x{mask:02X}", flush=True)
                print(f"CYCLE_END={cycle}", flush=True)
            await asyncio.sleep(2.0)
            await client.stop_notify(TELEMETRY)
            await client.stop_notify(STATUS)
        return 0
    finally:
        stop.set()
        reader.join(timeout=2.0)


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
