"""Small capability interfaces for the current single-leg experiment."""

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class LegCommand:
    """Motion intent sent from control logic to a platform adapter."""

    mode: str
    progress: float = 0.0


class LegActuator(Protocol):
    def command_leg(self, command: LegCommand) -> None:
        """Execute a hardware-independent motion intent."""


class FootContactSensor(Protocol):
    def read_contact(self) -> bool:
        """Return whether the foot is currently considered in contact."""


class PlayerInput(Protocol):
    def is_pressed(self) -> bool:
        """Return the current player input level."""


class BodyAttitudeSensor(Protocol):
    """Optional capability; not used by the v0 single-leg FSM."""

    def read_attitude(self) -> object:
        ...


class FeedbackDevice(Protocol):
    """Optional capability for display, LEDs, buzzer, scoring, etc."""

    def show(self, message: str) -> None:
        ...

