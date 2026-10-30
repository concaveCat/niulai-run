import unittest

from firmware.control.controller import SingleLegController
from firmware.control.leg_fsm import LegFsm, LegFsmConfig, LegState
from firmware.control.mock import MockFootContactSensor, MockLegActuator, MockPlayerInput


class LegFsmTests(unittest.TestCase):
    def press_and_reach_swing(self, fsm: LegFsm) -> None:
        fsm.update(True, False, 0.0)
        fsm.update(True, False, fsm.config.lift_duration_s)
        self.assertEqual(fsm.state, LegState.SWING)

    def test_normal_press_hold_release_contact_flow(self) -> None:
        fsm = LegFsm(LegFsmConfig(lift_duration_s=0.1, swing_speed=2.0, max_swing=1.0))
        self.press_and_reach_swing(fsm)
        command = fsm.update(True, False, 0.25)
        self.assertEqual(command.mode, "swing")
        self.assertEqual(command.progress, 0.5)
        self.assertEqual(fsm.update(False, False, 0.01).mode, "search")
        self.assertEqual(fsm.update(False, True, 0.01).mode, "hold_support")
        self.assertEqual(fsm.state, LegState.SUPPORT)

    def test_long_hold_is_capped_and_does_not_plan_another_leg(self) -> None:
        fsm = LegFsm(LegFsmConfig(lift_duration_s=0.1, swing_speed=1.0, max_swing=0.3))
        self.press_and_reach_swing(fsm)
        fsm.update(True, False, 10.0)
        self.assertEqual(fsm.state, LegState.SWING)
        self.assertEqual(fsm.swing_progress, 0.3)
        self.assertNotIn("gait", repr(fsm.transitions).lower())

    def test_release_without_contact_remains_in_search(self) -> None:
        fsm = LegFsm(LegFsmConfig(lift_duration_s=0.1))
        self.press_and_reach_swing(fsm)
        fsm.update(False, False, 0.1)
        fsm.update(False, False, 1.0)
        self.assertEqual(fsm.state, LegState.SEARCH)

    def test_state_timeout_enters_safe_fault(self) -> None:
        fsm = LegFsm(LegFsmConfig(lift_duration_s=5.0, state_timeout_s=0.5))
        fsm.update(True, False, 0.0)
        fsm.update(True, False, 0.6)
        self.assertEqual(fsm.state, LegState.FAULT)
        self.assertEqual(fsm.command().mode, "safe_stop")

    def test_invalid_input_is_logged_without_crashing(self) -> None:
        fsm = LegFsm()
        command = fsm.update("not-a-button", False, -1.0)
        self.assertEqual(command.mode, "hold_support")
        self.assertEqual(len(fsm.transitions), 2)
        self.assertTrue(all(item.reason == "invalid input ignored" for item in fsm.transitions))

    def test_controller_uses_only_replaceable_capabilities(self) -> None:
        fsm = LegFsm(LegFsmConfig(lift_duration_s=0.1))
        player = MockPlayerInput()
        contact = MockFootContactSensor()
        actuator = MockLegActuator()
        controller = SingleLegController(fsm, player, contact, actuator)
        controller.step(0.0)
        player.pressed = True
        controller.step(0.01)
        controller.step(0.1)
        self.assertEqual(actuator.commands[-1].mode, "swing")


if __name__ == "__main__":
    unittest.main()

