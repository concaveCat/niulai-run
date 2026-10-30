"""One-session diagnostic for the CRC UART path; never enables outputs."""

import sys
import time

import serial

from servo_uart_protocol import set_target_frame


port = sys.argv[1] if len(sys.argv) > 1 else "COM4"
with serial.Serial(port, 115200, timeout=0.05) as ser:
    ser.dtr = False
    ser.rts = False
    time.sleep(1.5)
    print("BOOT", ser.read(8192).hex())
    ser.write(set_target_frame(0x1234, [1746, 1469, 1427, 1415,
                                        1619, 1582, 1449, 1600]))
    ser.flush()
    time.sleep(1.0)
    print("AFTER", ser.read(8192).hex())
