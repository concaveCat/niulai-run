import unittest

from tools.servo_trace import (
    DeviceRx,
    HostTx,
    format_host_stats,
    interval_stats,
    parse_device_lines,
    summarize_device_lines,
)


class ServoTraceStatsTests(unittest.TestCase):
    def test_percentile_stats_are_stable(self):
        stats = interval_stats([19.0, 20.0, 21.0, 100.0])
        self.assertEqual(stats["min"], 19.0)
        self.assertEqual(stats["max"], 100.0)
        self.assertAlmostEqual(stats["p95"], 88.15)

    def test_device_batching_and_tick_lines_are_parsed(self):
        lines = [
            "ESP_RX seq=1 timestamp_us=1000 pulse=1500 rx_interval_us=0",
            "ESP_RX seq=2 timestamp_us=21000 pulse=1505 rx_interval_us=20000",
            "ESP_RX seq=3 timestamp_us=21200 pulse=1510 rx_interval_us=200",
            "CONTROL_TICK id=8 timestamp_us=10000 tick_interval_us=0",
            "CONTROL_TICK id=9 timestamp_us=20000 tick_interval_us=10000",
            "I2C_END seq=3 timestamp_us=22000 result=0x0 duration_us=800",
        ]
        rx, ticks, i2c = parse_device_lines(lines)
        self.assertEqual(rx, [
            DeviceRx(1, 1000, 1500, 0),
            DeviceRx(2, 21000, 1505, 20000),
            DeviceRx(3, 21200, 1510, 200),
        ])
        self.assertEqual(ticks, [0.0, 10.0])
        self.assertEqual(i2c, [0.8])
        summary = summarize_device_lines(lines)
        self.assertIn("ESP RX <=2 ms batches: 1", summary)
        self.assertIn("I2C duration ms", summary)

    def test_host_tx_stats_use_perf_counter_domain(self):
        records = [HostTx(1, 1_000_000, 1500), HostTx(2, 21_000_000, 1505)]
        self.assertIn("mean=20.000", format_host_stats(records))


if __name__ == "__main__":
    unittest.main()
