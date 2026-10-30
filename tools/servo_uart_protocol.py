"""CRC-framed UART transport for the isolated servo-zero firmware."""

from __future__ import annotations

import argparse
import struct
import time

try:
    import serial
except ImportError:  # pragma: no cover - depends on the host environment
    serial = None

SOF = 0xA5
ACK_SOF = 0x5A
VERSION = 1
SET_TARGET = 1
RUN = 2
STOP = 3
ACK = 0x80
NACK = 0x81
STATUS_NAMES = {
    0: "ok",
    1: "bad_crc",
    2: "bad_version",
    3: "bad_command",
    4: "bad_range",
    5: "busy",
    6: "io",
}


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _finish(body: bytes) -> bytes:
    return body + struct.pack("<H", crc16(body))


def set_target_frame(sequence: int, pulses: list[int], mask: int = 0xFF) -> bytes:
    if len(pulses) != 8:
        raise ValueError("exactly eight pulse values are required")
    body = struct.pack("<BBBHB8H", SOF, VERSION, SET_TARGET, sequence, mask, *pulses)
    return _finish(body)


def control_frame(command: int, sequence: int) -> bytes:
    return _finish(struct.pack("<BBBHB", SOF, VERSION, command, sequence, 0))


def decode_ack(frame: bytes) -> tuple[int, int]:
    if (
        len(frame) != 8
        or frame[0] != ACK_SOF
        or frame[1] != VERSION
        or frame[2] not in (ACK, NACK)
        or crc16(frame[:-2]) != struct.unpack_from("<H", frame, 6)[0]
    ):
        raise ValueError("invalid ACK frame")
    status = frame[5]
    if (frame[2] == ACK and status != 0) or (frame[2] == NACK and status == 0):
        raise ValueError("ACK/NACK status mismatch")
    return struct.unpack_from("<H", frame, 3)[0], status


def transact(port: str, frame: bytes, sequence: int, timeout: float = 2.0) -> int:
    if serial is None:
        raise RuntimeError("pyserial is required for UART transport")
    with serial.Serial(port, 115200, timeout=0.05) as ser:
        ser.dtr = False
        ser.rts = False
        time.sleep(0.8)
        ser.reset_input_buffer()
        ser.write(frame)
        ser.flush()
        buffer = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            buffer.extend(ser.read(64))
            while len(buffer) >= 8:
                try:
                    start = buffer.index(ACK_SOF)
                except ValueError:
                    buffer.clear()
                    break
                if start:
                    del buffer[:start]
                candidate = bytes(buffer[:8])
                if crc16(candidate[:-2]) != struct.unpack_from("<H", candidate, 6)[0]:
                    del buffer[0]
                    continue
                del buffer[:8]
                ack_sequence, status = decode_ack(candidate)
                if ack_sequence == sequence:
                    return status
        raise TimeoutError(f"no ACK for sequence {sequence}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyUSB0")
    sub = parser.add_subparsers(dest="command", required=True)
    target = sub.add_parser("set-target")
    target.add_argument("pulses", help="eight comma-separated pulse values")
    target.add_argument("--mask", type=lambda value: int(value, 0), default=0xFF)
    sub.add_parser("run")
    sub.add_parser("stop")
    args = parser.parse_args()
    if serial is None:
        print("UART transport failed: pyserial is not installed")
        return 2
    sequence = int(time.monotonic_ns() & 0xFFFF)
    if args.command == "set-target":
        pulses = [int(value) for value in args.pulses.split(",")]
        frame = set_target_frame(sequence, pulses, args.mask)
    else:
        frame = control_frame(RUN if args.command == "run" else STOP, sequence)
    status = transact(args.port, frame, sequence)
    print(f"ACK sequence={sequence} status={status} ({STATUS_NAMES.get(status, 'unknown')})")
    return 0 if status == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
