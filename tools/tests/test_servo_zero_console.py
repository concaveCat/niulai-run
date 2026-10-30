import os
import unittest
from unittest.mock import patch

from tools.servo_zero_console import (
    ServoZeroConsole,
    adjust_pulse,
    decode_posix_escape,
    pulse_to_ticks,
    read_posix_key,
)


class FakeSerial:
    def __init__(self, *args, **kwargs):
        self.writes = []
        self.rx = bytearray()
        self.dtr = False
        self.rts = False

    @property
    def in_waiting(self):
        return len(self.rx)

    def write(self, data):
        self.writes.append(data.decode("ascii"))
        if data == b"list\r\n":
            self.rx.extend(b"CH0 rf_hip  = 1500 us\r\n")
        return len(data)

    def flush(self):
        return None

    def read(self, size):
        data = bytes(self.rx[:size])
        del self.rx[:size]
        return data

    def readline(self):
        if b"\n" not in self.rx:
            return b""
        index = self.rx.index(b"\n") + 1
        return self.read(index)

    def close(self):
        return None


class ServoZeroConsoleMathTest(unittest.TestCase):
    def test_firmware_conversion_matches_pca9685(self):
        self.assertEqual(pulse_to_ticks(1500), 307)

    def test_fine_adjustment_changes_one_microsecond(self):
        lower = adjust_pulse(1500, -1)
        upper = adjust_pulse(1500, 1)
        self.assertEqual(lower, 1499)
        self.assertEqual(upper, 1501)

    def test_adjustment_is_clamped_to_extended_firmware_range(self):
        self.assertEqual(adjust_pulse(300, -100), 300)
        self.assertEqual(adjust_pulse(3000, 100), 3000)

    @patch("tools.servo_zero_console.time.sleep", return_value=None)
    @patch("tools.servo_zero_console.serial.Serial")
    def test_console_starts_in_direct_channel_control(self, serial_open, _sleep):
        fake = FakeSerial()
        serial_open.return_value = fake
        console = ServoZeroConsole("COM4", 115200, 0.0)
        self.assertEqual(fake.writes[:3], ["stop\r\n", "list\r\n", "stop\r\n"])

        console.handle("1")
        console.handle("RIGHT")

        self.assertEqual(console.selected, 1)
        self.assertIn("pulse 1501\r\n", fake.writes)
        console.close()

    def test_arrow_escape_sequences_are_not_treated_as_exit(self):
        self.assertEqual(decode_posix_escape("[D"), "LEFT")
        self.assertEqual(decode_posix_escape("[C"), "RIGHT")
        self.assertEqual(decode_posix_escape("OA"), "UP")
        self.assertEqual(decode_posix_escape("[unknown"), "")
        self.assertEqual(decode_posix_escape(""), "ESC")

    def test_unbuffered_posix_reader_consumes_complete_arrow_sequence(self):
        read_fd, write_fd = os.pipe()
        try:
            os.write(write_fd, b"\x1b[D")
            self.assertEqual(read_posix_key(read_fd), "LEFT")
        finally:
            os.close(read_fd)
            os.close(write_fd)

if __name__ == "__main__":
    unittest.main()
