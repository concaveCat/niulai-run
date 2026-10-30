#!/usr/bin/env python3
"""Read-only BLE discovery probe for the ESP32 diagnostic image."""

import asyncio
import sys

from bleak import BleakClient, BleakScanner

TARGET_NAME = "牛来快跑"
TARGET_UUID = "7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11"


async def main() -> int:
    devices = await BleakScanner.discover(timeout=8.0)
    target = next(
        (d for d in devices if d.name == TARGET_NAME),
        None,
    )
    if target is None:
        print("牛来快跑 not found")
        return 2
    print(f"TARGET address={target.address} name={target.name}")
    async with BleakClient(target) as client:
        print(f"CONNECTED={client.is_connected}")
        for service in client.services:
            print(f"SERVICE {service.uuid}")
            for characteristic in service.characteristics:
                print(
                    f"CHAR {characteristic.uuid} "
                    f"properties={','.join(characteristic.properties)}"
                )
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
