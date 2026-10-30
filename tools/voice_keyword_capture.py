#!/usr/bin/env python3
"""Capture the isolated ESP32 voice-test PCM stream into a WAV file."""

import argparse
import math
import struct
import time
import wave

import serial


MAGIC = b"QWVCAP1\0"
HEADER_SIZE = 20


def read_capture(ser, timeout=20.0):
    # Opening a native Windows COM port can toggle RTS/DTR and reset the board.
    # Let the voice task reach its command loop, then discard boot text before
    # looking for the binary capture header.
    time.sleep(1.0)
    ser.reset_input_buffer()
    ser.write(b"voice capture\n")
    ser.flush()
    data = bytearray()
    deadline = time.monotonic() + timeout
    while MAGIC not in data:
        if time.monotonic() >= deadline:
            raise TimeoutError("did not receive the QWVCAP1 capture header")
        chunk = ser.read(4096)
        if chunk:
            data.extend(chunk)

    start = data.index(MAGIC)
    del data[:start]
    while len(data) < HEADER_SIZE:
        if time.monotonic() >= deadline:
            raise TimeoutError("capture header was truncated")
        data.extend(ser.read(4096))

    magic, rate, samples, bits, channels = struct.unpack(
        "<8sIIHH", data[:HEADER_SIZE]
    )
    if magic != MAGIC or bits != 16 or channels != 1:
        raise ValueError(
            f"unexpected capture format: magic={magic!r} rate={rate} "
            f"bits={bits} channels={channels}"
        )

    payload_size = samples * 2
    while len(data) < HEADER_SIZE + payload_size:
        if time.monotonic() >= deadline:
            raise TimeoutError("capture payload was truncated")
        data.extend(ser.read(4096))
    return rate, bytes(data[HEADER_SIZE : HEADER_SIZE + payload_size])


def pcm_stats(payload):
    samples = struct.unpack("<%dh" % (len(payload) // 2), payload)
    if not samples:
        raise ValueError("capture contains no samples")
    peak = max(abs(sample) for sample in samples)
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples))
    crossings = sum(
        1
        for left, right in zip(samples, samples[1:])
        if (left < 0 <= right) or (left >= 0 > right)
    )
    return {
        "samples": len(samples),
        "min": min(samples),
        "max": max(samples),
        "peak": peak,
        "rms": rms,
        "zero_crossings": crossings,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--output", default="niulai-voice-capture.wav")
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    with serial.Serial(args.port, 115200, timeout=0.2) as ser:
        ser.dtr = False
        ser.rts = False
        rate, payload = read_capture(ser, args.timeout)

    with wave.open(args.output, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(payload)

    stats = pcm_stats(payload)
    print(
        "VOICE_CAPTURE_SAVED output=%s rate=%u samples=%u min=%d max=%d "
        "peak=%d rms=%.1f zero_crossings=%u"
        % (
            args.output,
            rate,
            stats["samples"],
            stats["min"],
            stats["max"],
            stats["peak"],
            stats["rms"],
            stats["zero_crossings"],
        )
    )


if __name__ == "__main__":
    main()
