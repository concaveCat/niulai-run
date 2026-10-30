"""Thin composition layer connecting the pure FSM to replaceable capabilities."""

from .interfaces import FootContactSensor, LegActuator, PlayerInput
from .leg_fsm import LegFsm


class SingleLegController:
    def __init__(
        self,
        fsm: LegFsm,
        player_input: PlayerInput,
        foot_contact: FootContactSensor,
        actuator: LegActuator,
    ) -> None:
        self.fsm = fsm
        self.player_input = player_input
        self.foot_contact = foot_contact
        self.actuator = actuator

    def step(self, dt_s: float) -> None:
        command = self.fsm.update(
            self.player_input.is_pressed(),
            self.foot_contact.read_contact(),
            dt_s,
        )
        self.actuator.command_leg(command)

