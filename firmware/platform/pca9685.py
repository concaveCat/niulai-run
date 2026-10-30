"""Parameterized PCA9685 and servo adapter.

This module intentionally stops at an injected I2C bus.  It is usable for
protocol tests and as a platform boundary, but it is not an ESP-IDF hardware
port.  Servo calibration is mandatory for angle commands; missing values are
represented as ``None``/TBD and never replaced with guessed defaults.
"""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Callable, Protocol

from .defaults import (
    PCA9685_DEFAULT_ADDRESS,
    PCA9685_DEFAULT_OSCILLATOR_HZ,
    PCA9685_DEFAULT_PWM_FREQUENCY_HZ,
    SERVO_DEFAULT_CENTER_PULSE_US,
    SERVO_DEFAULT_DIRECTION,
    SERVO_DEFAULT_MAX_ANGLE_DEG,
    SERVO_DEFAULT_MAX_PULSE_US,
    SERVO_DEFAULT_MIN_ANGLE_DEG,
    SERVO_DEFAULT_MIN_PULSE_US,
    SERVO_DEFAULT_ZERO_ANGLE_DEG,
)


class I2cWriteBus(Protocol):
    """Minimal bus capability needed by this driver."""

    def write(self, address: int, data: bytes) -> None:
        ...


class ParameterTBDError(ValueError):
    """Raised when a physical parameter has not been filled in yet."""


class Pca9685Error(ValueError):
    """Raised for an invalid PCA9685 register-level operation."""


@dataclass(frozen=True)
class Pca9685Config:
    """PCA9685 bus settings.

    ``frequency_hz`` remains optional because the servo frequency is a
    project parameter still marked TBD.  The oscillator value is the
    PCA9685 nominal 25 MHz value used by the register formula.
    """

    address: int = PCA9685_DEFAULT_ADDRESS
    oscillator_hz: float = PCA9685_DEFAULT_OSCILLATOR_HZ
    frequency_hz: float | None = PCA9685_DEFAULT_PWM_FREQUENCY_HZ

    def __post_init__(self) -> None:
        if not 0x03 <= self.address <= 0x77:
            raise Pca9685Error("I2C address must be a 7-bit address")
        if not math.isfinite(self.oscillator_hz) or self.oscillator_hz <= 0:
            raise Pca9685Error("oscillator_hz must be finite and positive")
        if self.frequency_hz is not None:
            _validate_frequency(self.frequency_hz)


@dataclass(frozen=True)
class ServoCalibration:
    """One servo's physical calibration; every field is TBD by default."""

    channel: int | None = None
    min_pulse_us: float | None = None
    center_pulse_us: float | None = None
    max_pulse_us: float | None = None
    zero_angle_deg: float | None = None
    direction: int | None = None
    min_angle_deg: float | None = None
    max_angle_deg: float | None = None

    @classmethod
    def generic_default(cls, channel: int) -> "ServoCalibration":
        """Return an explicit common-servo candidate for bring-up/mock use."""

        return cls(
            channel=channel,
            min_pulse_us=SERVO_DEFAULT_MIN_PULSE_US,
            center_pulse_us=SERVO_DEFAULT_CENTER_PULSE_US,
            max_pulse_us=SERVO_DEFAULT_MAX_PULSE_US,
            zero_angle_deg=SERVO_DEFAULT_ZERO_ANGLE_DEG,
            direction=SERVO_DEFAULT_DIRECTION,
            min_angle_deg=SERVO_DEFAULT_MIN_ANGLE_DEG,
            max_angle_deg=SERVO_DEFAULT_MAX_ANGLE_DEG,
        )

    def pulse_for_angle(self, angle_deg: float) -> float:
        """Map a joint angle to pulse width after complete calibration.

        The map is piecewise-linear around the calibrated mechanical zero so
        asymmetric pulse ranges and asymmetric angle limits are supported.
        ``direction=1`` maps increasing angle toward max pulse; ``-1`` maps
        it toward min pulse.
        """

        values = (
            self.min_pulse_us,
            self.center_pulse_us,
            self.max_pulse_us,
            self.zero_angle_deg,
            self.min_angle_deg,
            self.max_angle_deg,
        )
        if any(value is None for value in values) or self.direction is None:
            raise ParameterTBDError("servo calibration is incomplete (TBD)")
        if self.direction not in (-1, 1):
            raise Pca9685Error("servo direction must be -1 or 1")
        assert self.min_pulse_us is not None
        assert self.center_pulse_us is not None
        assert self.max_pulse_us is not None
        assert self.zero_angle_deg is not None
        assert self.min_angle_deg is not None
        assert self.max_angle_deg is not None
        if not all(math.isfinite(value) for value in values if value is not None):
            raise Pca9685Error("servo calibration values must be finite")
        if not self.min_pulse_us <= self.center_pulse_us <= self.max_pulse_us:
            raise Pca9685Error("pulse range must be min <= center <= max")
        if not self.min_angle_deg < self.zero_angle_deg < self.max_angle_deg:
            raise Pca9685Error("zero angle must be inside mechanical limits")
        if not self.min_angle_deg <= angle_deg <= self.max_angle_deg:
            raise Pca9685Error("angle exceeds calibrated mechanical limits")

        if angle_deg >= self.zero_angle_deg:
            span = self.max_angle_deg - self.zero_angle_deg
            fraction = (angle_deg - self.zero_angle_deg) / span
            endpoint = (
                self.max_pulse_us if self.direction == 1 else self.min_pulse_us
            )
        else:
            span = self.zero_angle_deg - self.min_angle_deg
            fraction = (self.zero_angle_deg - angle_deg) / span
            endpoint = (
                self.min_pulse_us if self.direction == 1 else self.max_pulse_us
            )
        return self.center_pulse_us + fraction * (endpoint - self.center_pulse_us)

    def require_complete(self) -> int:
        """Validate calibration and return the configured PCA channel."""

        if self.channel is None:
            raise ParameterTBDError("servo PCA channel is TBD")
        if not 0 <= self.channel <= 15:
            raise Pca9685Error("PCA9685 channel must be between 0 and 15")
        # Use a harmless angle only to run the common completeness checks.
        self.pulse_for_angle(self.zero_angle_deg if self.zero_angle_deg is not None else 0.0)
        return self.channel


def _validate_frequency(frequency_hz: float) -> None:
    if not math.isfinite(frequency_hz) or frequency_hz <= 0:
        raise Pca9685Error("frequency_hz must be finite and positive")


class Pca9685Driver:
    """PCA9685 register driver with no hidden servo defaults."""

    MODE1 = 0x00
    MODE2 = 0x01
    LED0_ON_L = 0x06
    ALL_LED_OFF_L = 0xFC
    PRESCALE = 0xFE

    MODE1_SLEEP = 0x10
    MODE1_RESTART = 0x80
    MODE1_AI = 0x20
    MODE2_OUTDRV = 0x04
    FULL_OFF = 0x10

    def __init__(
        self,
        bus: I2cWriteBus,
        config: Pca9685Config | None = None,
        sleep_us: Callable[[int], None] | None = None,
    ) -> None:
        self.bus = bus
        self.config = config or Pca9685Config()
        self._sleep_us = sleep_us
        self._frequency_hz = self.config.frequency_hz

    @property
    def frequency_hz(self) -> float | None:
        return self._frequency_hz

    def _write_register(self, register: int, *values: int) -> None:
        if not 0 <= register <= 0xFF:
            raise Pca9685Error("register must be an 8-bit value")
        if any(not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= 0xFF for value in values):
            raise Pca9685Error("register data must contain 8-bit integers")
        self.bus.write(self.config.address, bytes((register, *values)))

    def _prescale_for_frequency(self, frequency_hz: float) -> int:
        _validate_frequency(frequency_hz)
        prescale = round(self.config.oscillator_hz / (4096.0 * frequency_hz) - 1.0)
        if not 3 <= prescale <= 255:
            raise Pca9685Error("frequency is outside the PCA9685 prescale range")
        return prescale

    def initialize(self, frequency_hz: float | None = None) -> int:
        """Configure frequency and leave all channels fully off.

        A frequency must be supplied either here or in ``Pca9685Config``.
        The returned value is the programmed PRESCALE register.
        """

        selected_frequency = frequency_hz if frequency_hz is not None else self.config.frequency_hz
        if selected_frequency is None:
            raise ParameterTBDError("PCA9685 PWM frequency is TBD")
        prescale = self._prescale_for_frequency(selected_frequency)
        self._write_register(self.MODE1, self.MODE1_SLEEP)
        self._write_register(self.PRESCALE, prescale)
        if self._sleep_us is not None:
            self._sleep_us(500)
        self._write_register(self.MODE1, self.MODE1_RESTART | self.MODE1_AI)
        self._write_register(self.MODE2, self.MODE2_OUTDRV)
        self.all_channels_off()
        self._frequency_hz = selected_frequency
        return prescale

    def set_pwm_frequency(self, frequency_hz: float) -> int:
        """Set PWM frequency using the PCA9685 PRESCALE formula."""

        return self.initialize(frequency_hz)

    def set_pwm(self, channel: int, on_tick: int, off_tick: int) -> None:
        """Write one channel's 12-bit ON/OFF counters."""

        _validate_channel(channel)
        _validate_tick(on_tick)
        _validate_tick(off_tick)
        register = self.LED0_ON_L + 4 * channel
        self._write_register(
            register,
            on_tick & 0xFF,
            (on_tick >> 8) & 0x0F,
            off_tick & 0xFF,
            (off_tick >> 8) & 0x0F,
        )

    def set_channel_off(self, channel: int) -> None:
        """Force one channel off using its FULL_OFF bit."""

        _validate_channel(channel)
        register = self.LED0_ON_L + 4 * channel
        self._write_register(register, 0, 0, 0, self.FULL_OFF)

    def all_channels_off(self) -> None:
        """Force all PCA channels off."""

        self._write_register(self.ALL_LED_OFF_L, 0, 0, 0, self.FULL_OFF)

    def pulse_width_to_ticks(self, pulse_us: float) -> int:
        """Convert a pulse width using the configured PWM frequency."""

        if self._frequency_hz is None:
            raise ParameterTBDError("PCA9685 PWM frequency is TBD")
        if not math.isfinite(pulse_us) or pulse_us <= 0:
            raise Pca9685Error("pulse_us must be finite and positive")
        ticks = round(pulse_us * self._frequency_hz * 4096.0 / 1_000_000.0)
        if not 1 <= ticks <= 4095:
            raise Pca9685Error("pulse width is outside the 12-bit PWM period")
        return ticks

    def set_pulse_width_us(self, channel: int, pulse_us: float) -> int:
        """Write a raw pulse width; calibration remains the caller's duty."""

        ticks = self.pulse_width_to_ticks(pulse_us)
        self.set_pwm(channel, 0, ticks)
        return ticks

    def set_servo_angle(self, calibration: ServoCalibration, angle_deg: float) -> int:
        """Write a calibrated servo angle, refusing all TBD parameters."""

        channel = calibration.require_complete()
        pulse_us = calibration.pulse_for_angle(angle_deg)
        return self.set_pulse_width_us(channel, pulse_us)


def _validate_channel(channel: int) -> None:
    if not isinstance(channel, int) or isinstance(channel, bool) or not 0 <= channel <= 15:
        raise Pca9685Error("PCA9685 channel must be between 0 and 15")


def _validate_tick(tick: int) -> None:
    if not isinstance(tick, int) or isinstance(tick, bool) or not 0 <= tick <= 4095:
        raise Pca9685Error("PWM tick must be an integer from 0 to 4095")
