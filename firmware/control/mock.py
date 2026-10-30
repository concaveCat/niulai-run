"""Mock capabilities for development without an assembled robot."""

from typing import List

from .interfaces import LegCommand


class MockPlayerInput:
    def __init__(self) -> None:
        self.pressed = False

    def is_pressed(self) -> bool:
        return self.pressed


class MockFootContactSensor:
    def __init__(self) -> None:
        self.contact = False

    def read_contact(self) -> bool:
        return self.contact


class MockLegActuator:
    def __init__(self) -> None:
        self.commands: List[LegCommand] = []

    def command_leg(self, command: LegCommand) -> None:
        self.commands.append(command)

