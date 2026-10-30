#!/usr/bin/env python3
"""Recalculate selected 3.3 V digital-interface and I2C margins.

Sources and variables are documented in calculations/README.md. Values are
datasheet guaranteed limits unless a design bound is explicitly named.
"""

from __future__ import annotations


def require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def volts(value: float) -> str:
    return f"{value:.3f} V"


def main() -> int:
    errors: list[str] = []

    # Common rail bounds from the intersection of device operating ranges.
    esp_vdd_min = 3.0
    esp_vdd_max = 3.6
    ws_vdd_min = 3.3
    ws_vdd_max = 5.3
    ws_common_min = max(esp_vdd_min, ws_vdd_min)
    ws_common_max = min(esp_vdd_max, ws_vdd_max)
    require(ws_common_min <= ws_common_max, "ESP/WS2812 rail ranges do not overlap", errors)

    # Same-rail WS2812C input margins. Both thresholds scale with the same VDD.
    esp_voh_ratio_min = 0.80
    esp_vol_ratio_max = 0.10
    ws_vih_ratio_min = 0.55
    ws_vil_max = 0.70
    ws_high_margin = (esp_voh_ratio_min - ws_vih_ratio_min) * ws_common_min
    ws_low_margin = ws_vil_max - esp_vol_ratio_max * ws_common_max
    require(ws_high_margin > 0.0, "WS2812 high-level margin is not positive", errors)
    require(ws_low_margin > 0.0, "WS2812 low-level margin is not positive", errors)

    led_count = 4
    led_channels = 3
    led_channel_current_max = 0.005
    led_current_max = led_count * led_channels * led_channel_current_max
    led_power_max = led_current_max * ws_common_max

    # PDM clock: ESP output to microphone input on the same rail.
    mic_vdd_min = 1.62
    mic_vdd_max = 3.6
    pdm_common_min = max(esp_vdd_min, mic_vdd_min)
    pdm_common_max = min(esp_vdd_max, mic_vdd_max)
    mic_vih_ratio_min = 0.65
    mic_vil_ratio_max = 0.35
    pdm_clk_high_margin = (
        esp_voh_ratio_min - mic_vih_ratio_min
    ) * pdm_common_min
    pdm_clk_low_margin = (
        mic_vil_ratio_max - esp_vol_ratio_max
    ) * pdm_common_min

    # PDM data: microphone output to ESP input.
    mic_voh_drop_max = 0.45
    mic_vol_max = 0.45
    esp_vih_ratio_min = 0.75
    esp_vil_ratio_max = 0.25
    pdm_data_high_margin = (
        pdm_common_min - mic_voh_drop_max
        - esp_vih_ratio_min * pdm_common_min
    )
    pdm_data_low_margin = (
        esp_vil_ratio_max * pdm_common_min - mic_vol_max
    )
    for name, margin in {
        "PDM clock high": pdm_clk_high_margin,
        "PDM clock low": pdm_clk_low_margin,
        "PDM data high": pdm_data_high_margin,
        "PDM data low": pdm_data_low_margin,
    }.items():
        require(margin > 0.0, f"{name} margin is not positive", errors)

    pdm_clock_hz = 2_400_000
    mic_standard_clock_min_hz = 1_000_000
    mic_standard_clock_max_hz = 4_800_000
    require(
        mic_standard_clock_min_hz <= pdm_clock_hz <= mic_standard_clock_max_hz,
        "selected PDM clock is outside microphone standard mode",
        errors,
    )

    # I2C Fast-mode pull-up bounds (NXP UM10204 Rev. 7.0, equations 1 and 2).
    i2c_vdd_max = 3.6
    i2c_vol_max = 0.4
    i2c_iol_required = 0.003
    i2c_rise_time_max_s = 300e-9
    i2c_bus_cap_design_max_f = 100e-12
    i2c_rp_nominal = 2_200.0
    i2c_rp_tolerance = 0.01
    i2c_rp_min = i2c_rp_nominal * (1.0 - i2c_rp_tolerance)
    i2c_rp_max = i2c_rp_nominal * (1.0 + i2c_rp_tolerance)
    i2c_allowed_rp_min = (i2c_vdd_max - i2c_vol_max) / i2c_iol_required
    i2c_allowed_rp_max = (
        i2c_rise_time_max_s / (0.8473 * i2c_bus_cap_design_max_f)
    )
    i2c_worst_rise_time_s = (
        0.8473 * i2c_rp_max * i2c_bus_cap_design_max_f
    )
    i2c_sink_current_a = (i2c_vdd_max - i2c_vol_max) / i2c_rp_min
    require(
        i2c_rp_min >= i2c_allowed_rp_min,
        "I2C pull-up tolerance violates the minimum resistance",
        errors,
    )
    require(
        i2c_rp_max <= i2c_allowed_rp_max,
        "I2C pull-up tolerance violates the 100 pF/300 ns maximum resistance",
        errors,
    )

    if errors:
        print("Low-voltage digital checks: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Low-voltage digital checks: PASS")
    print(
        "- shared ESP32/WS2812 guaranteed steady rail intersection: "
        f"{volts(ws_common_min)} to {volts(ws_common_max)}"
    )
    print(f"- WS2812 DIN high margin at {ws_common_min:.1f} V: {volts(ws_high_margin)}")
    print(f"- WS2812 DIN low margin at {ws_common_max:.1f} V: {volts(ws_low_margin)}")
    print(f"- four-LED maximum channel current: {led_current_max * 1000:.1f} mA")
    print(f"- four-LED rail power at {ws_common_max:.1f} V: {led_power_max:.3f} W")
    print(f"- PDM CLK high/low margins: {volts(pdm_clk_high_margin)} / {volts(pdm_clk_low_margin)}")
    print(f"- PDM DATA high/low margins: {volts(pdm_data_high_margin)} / {volts(pdm_data_low_margin)}")
    print(f"- selected PDM clock: {pdm_clock_hz / 1e6:.1f} MHz (standard mode)")
    print(
        "- I2C allowed pull-up range at 3.6 V, 100 pF, Fast-mode: "
        f"{i2c_allowed_rp_min:.0f} to {i2c_allowed_rp_max:.0f} ohm"
    )
    print(
        "- selected I2C pull-up: 2.20 kohm +/-1%; worst rise time "
        f"{i2c_worst_rise_time_s * 1e9:.0f} ns; "
        f"sink current {i2c_sink_current_a * 1000:.2f} mA"
    )
    print(
        "- design-input consequence: the common LED/MCU rail has no "
        "guaranteed steady-state overlap below 3.300 V"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
