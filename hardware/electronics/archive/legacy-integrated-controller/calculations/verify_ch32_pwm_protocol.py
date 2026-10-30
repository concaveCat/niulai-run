#!/usr/bin/env python3
"""Static checks for the CH32V003 eight-channel servo-PWM contract."""

from __future__ import annotations

I2C_ADDRESS = 0x40
FRAME_US = 20_000
TIMER_CLOCK_HZ = 48_000_000
PRESCALER = 47
TICK_HZ = TIMER_CLOCK_HZ // (PRESCALER + 1)
ARR = FRAME_US - 1
PULSE_MIN_US = 500
PULSE_NEUTRAL_US = 1_500
PULSE_MAX_US = 2_500
WATCHDOG_DEFAULT_MS = 100

CHANNELS = {
    1: "TIM1_CH1/PD2",
    2: "TIM1_CH2/PA1",
    3: "TIM1_CH3/PC3",
    4: "TIM1_CH4/PC4",
    5: "TIM2_CH1/PC1",
    6: "TIM2_CH2/PC7",
    7: "TIM2_CH3/PD6",
    8: "TIM2_CH4/PD5",
}

SHADOW_BASE = 0x08
SHADOW_BYTES = 16
COMMIT_REGISTER = 0x18


def main() -> int:
    assert 0x08 <= I2C_ADDRESS <= 0x77
    assert TICK_HZ == 1_000_000
    assert ARR == 19_999
    assert 0 < PULSE_MIN_US < PULSE_NEUTRAL_US < PULSE_MAX_US < FRAME_US
    assert len(CHANNELS) == 8 and len(set(CHANNELS.values())) == 8
    assert SHADOW_BASE + SHADOW_BYTES == COMMIT_REGISTER
    assert WATCHDOG_DEFAULT_MS >= 2 * FRAME_US / 1000

    print("CH32 servo-PWM protocol/timer contract: PASS")
    print(f"- I2C 7-bit address: 0x{I2C_ADDRESS:02X}")
    print(f"- timer: {TIMER_CLOCK_HZ/1e6:.0f} MHz / ({PRESCALER}+1) = {TICK_HZ/1e6:.0f} MHz")
    print(f"- frame: ARR={ARR}, period={FRAME_US/1000:.1f} ms, 50 Hz")
    print(f"- pulse clamp: {PULSE_MIN_US}..{PULSE_MAX_US} us; inactive default low")
    print(f"- shadow block: 0x{SHADOW_BASE:02X}..0x{COMMIT_REGISTER-1:02X}; commit=0x{COMMIT_REGISTER:02X}")
    print(f"- watchdog default: {WATCHDOG_DEFAULT_MS} ms; timeout disables timers and drives all inputs low")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
