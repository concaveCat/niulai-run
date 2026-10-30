import unittest

from tools.quad_ascii_walk import (
    QuadAsciiWalk,
    DEFAULT_PROFILE,
    PROFILE_40MM,
    PROFILE_HALF_HIP_40MM,
    PROFILE_KNEE_DOMINANT_40MM,
    PRESS_RETRY_TIMEOUT_S,
    PRESS_DEFERRED,
    PRESS_REJECTED,
    classify_quad_press_response,
    full_swing_hold_seconds,
    has_terminal_quad_failure,
    release_completion_observed,
)


class QuadAsciiWalkFailureTest(unittest.TestCase):
    def test_stale_terminal_failure_requires_new_rest_before_press(self):
        walker = QuadAsciiWalk.__new__(QuadAsciiWalk)
        walker.rest_ready = True
        sent = []
        walker._drain = lambda _seconds: [
            "quad demo stopped at 451 ms: invalid pose or I2C failure; /OE HIGH"
        ]
        walker._send = sent.append
        self.assertEqual(walker.press_status(1), PRESS_REJECTED)
        self.assertFalse(walker.rest_ready)
        self.assertEqual(sent, [])

    def test_stale_terminal_failure_is_not_discarded_on_release(self):
        walker = QuadAsciiWalk.__new__(QuadAsciiWalk)
        walker.rest_ready = True
        sent = []
        walker._drain = lambda _seconds: [
            "quad demo stopped at 451 ms: invalid pose or I2C failure; /OE HIGH"
        ]
        walker._send = sent.append
        self.assertFalse(walker.release(1))
        self.assertFalse(walker.rest_ready)
        self.assertEqual(sent, [])

    def test_default_profile_is_knee_dominant(self):
        self.assertEqual(DEFAULT_PROFILE, PROFILE_KNEE_DOMINANT_40MM)
        self.assertEqual(DEFAULT_PROFILE, "30,9,40,300,360,120,50")

    def test_forty_mm_clearance_profile_is_explicit(self):
        self.assertEqual(PROFILE_40MM, "30,9,40,300,360,120,25")

    def test_half_hip_forty_mm_assembly_profile_is_explicit(self):
        self.assertEqual(PROFILE_HALF_HIP_40MM,
                         "30,9,40,300,360,120,50")

    def test_press_retry_window_is_bounded(self):
        self.assertGreater(PRESS_RETRY_TIMEOUT_S, 0.0)
        self.assertLessEqual(PRESS_RETRY_TIMEOUT_S, 2.0)

    def test_sequence_hold_reaches_full_default_swing(self):
        self.assertAlmostEqual(
            full_swing_hold_seconds(DEFAULT_PROFILE), 0.7)

    def test_sequence_hold_keeps_invalid_profile_conservative(self):
        self.assertEqual(full_swing_hold_seconds("not,a,profile"), 0.5)

    def test_release_after_completion_does_not_wait_for_second_marker(self):
        self.assertTrue(release_completion_observed([
            "quad key release already applied: fixed landing continues",
            "quad release mask=0x01 accepted",
        ]))

    def test_release_completion_marker_is_success(self):
        self.assertTrue(release_completion_observed([
            "quad demo complete; soft_limit_legs=0x00",
        ]))

    def test_recoverable_i2c_retry_is_not_a_quad_failure(self):
        self.assertFalse(has_terminal_quad_failure([
            "active frame I2C failed on attempt 1/4: ESP_ERR_INVALID_STATE; retrying after 500 us",
            "quad key release: key_mask=0x02 released=0x02 fixed landing continues",
        ]))

    def test_nack_statistics_are_not_a_quad_failure(self):
        self.assertFalse(has_terminal_quad_failure([
            "I2C stats: logical=103 attempts=104 success=103 nack=1 retries=1 recovered_after_retry=1 terminal_failures=0",
        ]))

    def test_terminal_quad_failure_is_detected(self):
        self.assertTrue(has_terminal_quad_failure([
            "quad demo stopped at 420 ms: invalid pose or I2C failure; /OE HIGH",
        ]))

    def test_terminal_quad_failure_is_not_retried_as_busy(self):
        self.assertEqual(
            classify_quad_press_response([
                "quad demo stopped at 420 ms: invalid pose or I2C failure; /OE HIGH",
            ]),
            PRESS_REJECTED,
        )

    def test_busy_press_is_deferred_instead_of_rejected(self):
        self.assertEqual(
            classify_quad_press_response([
                "quad press mask=0x04 deferred; current motion/window is busy",
            ]),
            PRESS_DEFERRED,
        )

    def test_firmware_quad_press_rejection_is_not_queued(self):
        self.assertEqual(
            classify_quad_press_response([
                "quad press rejected: leg mask=0x01 is already landing",
            ]),
            PRESS_REJECTED,
        )

    def test_uncertain_pending_release_is_one_way_success(self):
        walker = QuadAsciiWalk.__new__(QuadAsciiWalk)
        walker.rest_ready = True
        sent = []
        walker._drain = lambda _seconds: []
        walker._send = sent.append
        self.assertTrue(walker.release(1, wait_for_complete=False))
        self.assertEqual(sent, ["demo quad release 1"])

    def test_invalid_press_configuration_is_rejected(self):
        self.assertEqual(
            classify_quad_press_response([
                "demo quad press/release rejected; use demo quad press MASK",
            ]),
            PRESS_REJECTED,
        )


if __name__ == "__main__":
    unittest.main()
