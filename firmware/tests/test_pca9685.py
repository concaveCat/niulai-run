import unittest

from firmware.platform.pca9685 import (
    ParameterTBDError,
    Pca9685Config,
    Pca9685Driver,
    Pca9685Error,
    ServoCalibration,
)


class RecordingI2cBus:
    def __init__(self) -> None:
        self.writes: list[tuple[int, bytes]] = []

    def write(self, address: int, data: bytes) -> None:
        self.writes.append((address, data))


class Pca9685Tests(unittest.TestCase):
    def test_initialize_programs_frequency_and_off_state(self) -> None:
        bus = RecordingI2cBus()
        driver = Pca9685Driver(bus, Pca9685Config(frequency_hz=50.0))

        self.assertEqual(driver.initialize(), 121)
        self.assertEqual(driver.frequency_hz, 50.0)
        self.assertEqual(
            bus.writes,
            [
                (0x40, bytes([0x00, 0x10])),
                (0x40, bytes([0xFE, 121])),
                (0x40, bytes([0x00, 0xA0])),
                (0x40, bytes([0x01, 0x04])),
                (0x40, bytes([0xFC, 0x00, 0x00, 0x00, 0x10])),
            ],
        )

    def test_channel_frame_encodes_12_bit_counters(self) -> None:
        bus = RecordingI2cBus()
        driver = Pca9685Driver(bus)

        driver.set_pwm(3, 0x0123, 0x0ABC)

        self.assertEqual(bus.writes[-1], (0x40, bytes([0x12, 0x23, 0x01, 0xBC, 0x0A])))

    def test_frequency_is_required_before_pulse_conversion(self) -> None:
        driver = Pca9685Driver(RecordingI2cBus(), Pca9685Config(frequency_hz=None))

        with self.assertRaises(ParameterTBDError):
            driver.set_pulse_width_us(0, 1500.0)

    def test_generic_frequency_default_is_explicit_and_overridable(self) -> None:
        driver = Pca9685Driver(RecordingI2cBus())

        self.assertEqual(driver.frequency_hz, 50.0)
        self.assertEqual(driver.initialize(), 121)

    def test_generic_servo_calibration_is_opt_in(self) -> None:
        bus = RecordingI2cBus()
        driver = Pca9685Driver(bus)
        calibration = ServoCalibration.generic_default(0)

        self.assertEqual(driver.set_servo_angle(calibration, 0.0), 307)

    def test_incomplete_servo_calibration_cannot_move_a_channel(self) -> None:
        bus = RecordingI2cBus()
        driver = Pca9685Driver(bus, Pca9685Config(frequency_hz=50.0))
        driver.initialize()
        writes_before = len(bus.writes)

        with self.assertRaises(ParameterTBDError):
            driver.set_servo_angle(ServoCalibration(), 0.0)

        self.assertEqual(len(bus.writes), writes_before)

    def test_complete_calibration_maps_zero_and_direction(self) -> None:
        bus = RecordingI2cBus()
        driver = Pca9685Driver(bus, Pca9685Config(frequency_hz=50.0))
        calibration = ServoCalibration(
            channel=2,
            min_pulse_us=1000.0,
            center_pulse_us=1500.0,
            max_pulse_us=2000.0,
            zero_angle_deg=0.0,
            direction=1,
            min_angle_deg=-90.0,
            max_angle_deg=90.0,
        )

        self.assertEqual(driver.set_servo_angle(calibration, 0.0), 307)
        self.assertEqual(bus.writes[-1], (0x40, bytes([0x0E, 0x00, 0x00, 0x33, 0x01])))

    def test_angle_outside_mechanical_limit_is_rejected(self) -> None:
        calibration = ServoCalibration(
            channel=0,
            min_pulse_us=1000.0,
            center_pulse_us=1500.0,
            max_pulse_us=2000.0,
            zero_angle_deg=0.0,
            direction=1,
            min_angle_deg=-30.0,
            max_angle_deg=30.0,
        )

        with self.assertRaises(Pca9685Error):
            calibration.pulse_for_angle(31.0)


if __name__ == "__main__":
    unittest.main()
