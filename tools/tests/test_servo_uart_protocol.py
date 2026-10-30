import unittest

from tools.servo_uart_protocol import (
    ACK_SOF,
    SET_TARGET,
    VERSION,
    control_frame,
    crc16,
    decode_ack,
    set_target_frame,
)


class ServoUartProtocolTest(unittest.TestCase):
    def test_crc_reference_vector(self):
        self.assertEqual(crc16(b"123456789"), 0x29B1)

    def test_set_target_frame_has_fixed_layout_and_crc(self):
        frame = set_target_frame(0x1234, [500, 600, 700, 800, 900, 1000, 1100, 1200])
        self.assertEqual(len(frame), 24)
        self.assertEqual(frame[:6], bytes([0xA5, VERSION, SET_TARGET, 0x34, 0x12, 0xFF]))
        self.assertEqual(crc16(frame[:-2]), int.from_bytes(frame[-2:], "little"))

    def test_control_frame_has_crc(self):
        frame = control_frame(2, 7)
        self.assertEqual(len(frame), 8)
        self.assertEqual(crc16(frame[:-2]), int.from_bytes(frame[-2:], "little"))

    def test_decode_ack_rejects_corruption(self):
        frame = bytes([ACK_SOF, VERSION, 0x80, 7, 0, 0])
        frame += crc16(frame).to_bytes(2, "little")
        self.assertEqual(decode_ack(frame), (7, 0))
        with self.assertRaises(ValueError):
            decode_ack(frame[:-1] + bytes([frame[-1] ^ 1]))

    def test_decode_ack_rejects_wrong_version_or_command(self):
        for version, command in ((VERSION + 1, 0x80), (VERSION, 0x7F)):
            body = bytes([ACK_SOF, version, command, 7, 0, 0])
            frame = body + crc16(body).to_bytes(2, "little")
            with self.assertRaises(ValueError):
                decode_ack(frame)

    def test_decode_ack_rejects_ack_nack_status_mismatch(self):
        for command, status in ((0x80, 1), (0x81, 0)):
            body = bytes([ACK_SOF, VERSION, command, 7, 0, status])
            frame = body + crc16(body).to_bytes(2, "little")
            with self.assertRaises(ValueError):
                decode_ack(frame)


if __name__ == "__main__":
    unittest.main()
