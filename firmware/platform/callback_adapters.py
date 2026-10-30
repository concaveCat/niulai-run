"""Generic hardware callback adapters.

These are intentionally boring: a board-specific integration can provide
callbacks without leaking a servo model or sensor bus into control logic.
"""

from typing import Callable

from ..control.interfaces import LegCommand


class CallbackPlayerInput:
    def __init__(self, read_pressed: Callable[[], bool]) -> None:
        self._read_pressed = read_pressed

    def is_pressed(self) -> bool:
        return bool(self._read_pressed())


class CallbackFootContactSensor:
    def __init__(self, read_contact: Callable[[], bool]) -> None:
        self._read_contact = read_contact

    def read_contact(self) -> bool:
        return bool(self._read_contact())


class CallbackLegActuator:
    def __init__(self, send_command: Callable[[LegCommand], None]) -> None:
        self._send_command = send_command

    def command_leg(self, command: LegCommand) -> None:
        self._send_command(command)

