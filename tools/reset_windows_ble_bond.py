#!/usr/bin/env python3
"""Remove only the Windows bond for a selected BLE device via WinRT.

The Bluetooth address is intentionally not stored in the repository. Pass it
explicitly, for example:

    python tools/reset_windows_ble_bond.py --address 0x001122334455
"""

import argparse
import asyncio
import os

from winrt.windows.devices.bluetooth import BluetoothLEDevice
from winrt.windows.devices.enumeration import DeviceInformation


async def remove_bond(address: int) -> int:
    device = await BluetoothLEDevice.from_bluetooth_address_async(address)
    if device is None:
        print("DEVICE_NOT_FOUND")
        return 2
    try:
        info = await DeviceInformation.create_from_id_async(
            device.device_information.id
        )
        pairing = info.pairing
        print(f"DEVICE={info.name} PAIRED={pairing.is_paired}")
        if pairing.is_paired:
            result = await pairing.unpair_async()
            print(f"UNPAIR_STATUS={result.status}")
            # WinRT's cached ``is_paired`` value can remain true until the
            # device object is refreshed, even after unpair_async succeeds.
        return 0
    finally:
        device.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--address",
        default=os.environ.get("NIULAI_BLE_ADDRESS", ""),
        help="Bluetooth address, e.g. 0x001122334455 (or NIULAI_BLE_ADDRESS)",
    )
    args = parser.parse_args()
    if not args.address:
        parser.error("--address is required")
    return asyncio.run(remove_bond(int(args.address, 0)))


if __name__ == "__main__":
    raise SystemExit(main())
