"""Centralized generic bring-up defaults.

These values are deliberately ordinary, replaceable starting points for
generic modules commonly sold for this project.  They are not measurements or
permanent hardware decisions.  Callers that control real hardware must keep
the calibration gate enabled until the parameter register is filled in.
"""

PCA9685_DEFAULT_ADDRESS = 0x40
PCA9685_DEFAULT_OSCILLATOR_HZ = 25_000_000.0
PCA9685_DEFAULT_PWM_FREQUENCY_HZ = 50.0

# Common hobby-servo starting point.  The actual servo may have a narrower
# safe range, so this is an explicit candidate rather than a hidden limit.
SERVO_DEFAULT_MIN_PULSE_US = 500.0
SERVO_DEFAULT_CENTER_PULSE_US = 1_500.0
SERVO_DEFAULT_MAX_PULSE_US = 2_500.0
SERVO_DEFAULT_ZERO_ANGLE_DEG = 0.0
SERVO_DEFAULT_DIRECTION = 1
SERVO_DEFAULT_MIN_ANGLE_DEG = -90.0
SERVO_DEFAULT_MAX_ANGLE_DEG = 90.0

# Candidate sequential mapping for the eight planned outputs.
SERVO_DEFAULT_CHANNELS = tuple(range(8))

# ST7789 170x320 first-pass candidates.  The x offset is common for panels
# built from a 240-pixel-wide controller RAM; panel-specific init may override
# it.  The backlight level is a logic-level candidate, not a voltage claim.
ST7789_DEFAULT_SPI_MODE = 0
ST7789_DEFAULT_SPI_HZ = 8_000_000
ST7789_DEFAULT_PIXEL_FORMAT = 0x55  # RGB565 / 16 bits
ST7789_DEFAULT_MADCTL = 0x00
ST7789_DEFAULT_X_OFFSET = 35
ST7789_DEFAULT_Y_OFFSET = 0
ST7789_DEFAULT_BACKLIGHT_ACTIVE_LEVEL = 1

# First-pass digital-audio candidates.  The shared BCLK/WS can use 32-bit
# slots while the two devices retain their different likely valid data widths.
I2S_DEFAULT_SAMPLE_RATE_HZ = 48_000
I2S_DEFAULT_SLOT_WIDTH_BITS = 32
I2S_DEFAULT_STANDARD = "i2s"
I2S_DEFAULT_BCLK_SAMPLE_EDGE = "rising"
MAX98357A_DEFAULT_DATA_BITS = 16
MAX98357A_DEFAULT_CHANNEL = "left"
MS4030_DEFAULT_DATA_BITS = 24
MS4030_DEFAULT_LR_LEVEL = 0  # low selects left in the public module manual

# KW10/MPU bring-up candidates.  FOOT GPIO assignment remains intentionally
# absent; these values describe only input semantics and debounce behavior.
KW10_DEFAULT_PULL = "up"
KW10_DEFAULT_ACTIVE_LEVEL = 0
KW10_DEFAULT_DEBOUNCE_MS = 20
MPU6500_DEFAULT_I2C_ADDRESS = 0x68
MPU6500_DEFAULT_AD0_LEVEL = 0
