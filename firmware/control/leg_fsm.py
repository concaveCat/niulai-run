"""Pure, hardware-independent Hold-to-Swing leg state machine."""

from dataclasses import dataclass, field
from enum import Enum
import math
from typing import List

from .interfaces import LegCommand


class LegState(str, Enum):
    SUPPORT = "SUPPORT"
    LIFT = "LIFT"
    SWING = "SWING"
    SEARCH = "SEARCH"
    FAULT = "FAULT"


@dataclass(frozen=True)
class LegFsmConfig:
    """Tunable experiment parameters, deliberately not final hardware specs."""

    lift_duration_s: float = 0.20
    swing_speed: float = 1.0
    search_speed: float = 0.5
    max_swing: float = 1.0
    state_timeout_s: float | None = None


@dataclass(frozen=True)
class Transition:
    from_state: LegState
    to_state: LegState
    reason: str
    elapsed_s: float


@dataclass
class LegFsm:
    config: LegFsmConfig = field(default_factory=LegFsmConfig)
    state: LegState = LegState.SUPPORT
    state_elapsed_s: float = 0.0
    swing_progress: float = 0.0
    previous_pressed: bool = False
    transitions: List[Transition] = field(default_factory=list)

    def __post_init__(self) -> None:
        self._validate_config()

    def _validate_config(self) -> None:
        values = (
            self.config.lift_duration_s,
            self.config.swing_speed,
            self.config.search_speed,
            self.config.max_swing,
        )
        if any(not math.isfinite(value) or value < 0 for value in values):
            raise ValueError("FSM parameters must be finite and non-negative")
        timeout = self.config.state_timeout_s
        if timeout is not None and (not math.isfinite(timeout) or timeout <= 0):
            raise ValueError("state_timeout_s must be positive or None")

    def reset(self) -> None:
        """Explicitly leave FAULT for a new experiment run."""
        self.state = LegState.SUPPORT
        self.state_elapsed_s = 0.0
        self.swing_progress = 0.0
        self.previous_pressed = False

    def update(self, pressed: object, contact: object, dt_s: object) -> LegCommand:
        """Advance one tick and return a hardware-independent motion command.

        Invalid input is logged as a no-op rather than being allowed to crash the
        prototype loop. Concrete safety validation belongs in the platform layer.
        """
        invalid_level = not isinstance(pressed, bool) or not isinstance(contact, bool)
        if invalid_level:
            self._record_invalid_input()
            # Ignore malformed levels instead of treating arbitrary truthy
            # objects as a real player edge or a real foot contact.
            pressed = self.previous_pressed
            contact = False
        if not isinstance(dt_s, (int, float)) or isinstance(dt_s, bool) or not math.isfinite(dt_s) or dt_s < 0:
            self._record_invalid_input()
            dt = 0.0
        else:
            dt = float(dt_s)

        self.state_elapsed_s += dt
        rising_edge = pressed and not self.previous_pressed
        self.previous_pressed = pressed

        if self.state is LegState.SUPPORT and rising_edge:
            self._transition(LegState.LIFT, "button press")
        elif self.state is LegState.LIFT:
            if self._timed_out():
                self._transition(LegState.FAULT, "state timeout")
            elif self.state_elapsed_s >= self.config.lift_duration_s:
                self._transition(LegState.SWING if pressed else LegState.SEARCH, "lift complete")
        elif self.state is LegState.SWING:
            if not pressed:
                self._transition(LegState.SEARCH, "button release")
            else:
                self.swing_progress = min(
                    self.config.max_swing,
                    self.swing_progress + dt * self.config.swing_speed,
                )
                if self._timed_out():
                    self._transition(LegState.FAULT, "state timeout")
        elif self.state is LegState.SEARCH:
            if contact:
                self._transition(LegState.SUPPORT, "foot contact")
            elif self._timed_out():
                self._transition(LegState.FAULT, "state timeout")

        return self.command()

    def command(self) -> LegCommand:
        if self.state is LegState.SUPPORT:
            return LegCommand("hold_support")
        if self.state is LegState.LIFT:
            lift_progress = min(
                1.0,
                self.state_elapsed_s / self.config.lift_duration_s,
            ) if self.config.lift_duration_s else 1.0
            return LegCommand("lift", lift_progress)
        if self.state is LegState.SWING:
            return LegCommand("swing", self.swing_progress)
        if self.state is LegState.SEARCH:
            return LegCommand("search", self.state_elapsed_s * self.config.search_speed)
        return LegCommand("safe_stop")

    def _timed_out(self) -> bool:
        timeout = self.config.state_timeout_s
        return timeout is not None and self.state_elapsed_s > timeout

    def _transition(self, to_state: LegState, reason: str) -> None:
        from_state = self.state
        elapsed_s = self.state_elapsed_s
        self.state = to_state
        self.state_elapsed_s = 0.0
        if to_state is LegState.SWING:
            self.swing_progress = 0.0
        self.transitions.append(Transition(from_state, to_state, reason, elapsed_s))

    def _record_invalid_input(self) -> None:
        self.transitions.append(Transition(self.state, self.state, "invalid input ignored", self.state_elapsed_s))
