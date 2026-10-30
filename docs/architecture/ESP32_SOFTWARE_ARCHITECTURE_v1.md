# ESP32 Software Architecture v1

Status: Testing on `fw/esp32-layered-runtime`.

## Goal

Keep board and device replacement below the control boundary. The game and
robot application must not depend on PCA9685 channels, servo pulse widths,
ST7789 commands, GPIO numbers, or an experimental gait strategy.

## Runtime layers

```text
Application / robot runtime
        ↓
Control and motion domain
        ↓
Platform services and capabilities
        ↓
Device drivers
        ↓
Selective HAL + ESP-IDF/BSP
        ↓
Physical hardware
```

### BSP and selective HAL

ESP-IDF and FreeRTOS remain the board platform. A small pure-C HAL is added
only where host fakes, bus fault injection, or board replacement is useful.
The first implementation slice is `components/qw_hal` plus the ESP-IDF
adapter `components/qw_hal_espidf`:

- `qw_hal_i2c_device_t` for register devices;
- `qw_hal_gpio_t` for inputs, outputs, and `/OE`;
- `qw_hal_clock_t` for monotonic time and device-scale microsecond delays;
- `qw_hal_spi_device_t` for display transport;
- storage and transport interfaces only when a consumer needs replacement or
  deterministic tests.

The HAL does not contain PCA9685, ST7789, BLE, or gameplay semantics.

### Device drivers

Drivers implement device protocols over the HAL or the ESP-IDF transport. The
current migration has moved PCA9685 I²C/GPIO/clock operations, KW10 GPIO,
ST7789 GPIO/SPI/clock operations, and MPU6500 register I²C operations behind
the adapter:

- PCA9685 and servo output;
- ST7789 panel;
- KW10 GPIO/contact sampling;
- MPU6500;
- I²S microphone and audio output;
- BLE GATT endpoint.

Device drivers must not select a leg, plan a gait, score a game, or call the
game layer.

PCA9685, ST7789, MPU6500 and KW10 expose explicit adapter-binding seams for
host/bring-up fault injection. Production wrappers may still construct the
ESP-IDF bus, but protocol logic can be exercised with supplied HAL
capabilities rather than an implicit platform singleton.

### Platform services

Services expose replaceable capabilities. The current production HUD path now
uses `display_service`, which validates portrait `170×320` versus landscape
`320×170`, owns chunking and RGB565 wire byte order, and leaves the framebuffer
unchanged:

- player input and BLE watchdog;
- debounced foot contact;
- actuator output and interpolation;
- display/HUD snapshots;
- audio playback;
- telemetry and diagnostics.

The normal application wiring now instantiates the concrete runtime services:
`i2c_bus_service` owns the shared I²C master for PCA9685 and MPU6500,
`runtime_i2s` serializes the shared MS4030/MAX98357A clock domain,
`runtime_imu` publishes a thread-safe read-only dynamic-acceleration telemetry snapshot
after a short static gravity-baseline calibration,
and `display_hud` consumes it together with BLE/motion/contact/safety snapshots
in the fixed 320×170 landscape profile. The HUD shows acceleration only; it
cannot authorize motion or alter safety state.
The normal voice listener is also telemetry/action-isolated: a keyword can
queue the configured audio response, but never changes BLE input, motion state,
or actuator output.

The display service binds portrait/landscape profiles to the ST7789 MADCTL
row/column-exchange bit, checks offset arithmetic, and owns one persistent
4096-byte transfer buffer per service instance. It does not allocate on every
frame.

The display service consumes a read-only status snapshot. It never authorizes
motion. Audio, voice, and HUD failures must not bypass or weaken the safety
gate.

### Control and motion domain

The control domain is hardware-independent. `motion_strategy` is the explicit
replacement seam; normal runtime selects the formal `ARD-1 /
PastureStep-Reserve` strategy, while the current `leg_fsm` implementation is
retained as a compatibility/isolated-test strategy:

- the existing single-leg `leg_fsm` through `motion_strategy`;
- `control_glue` composition;
- per-leg motion intent and future four-leg motion strategy;
- IK and trajectory math;
- replaceable `LegActuator`, `FootContactSensor`, and `PlayerInput`
  capabilities.

The ARD-1 strategy emits public `FL/FR/RL/RR` joint targets and a generic
snapshot. Its profile, pulse map, and physical channel table stay below the
actuator adapter and must not leak into gameplay. The project continues to
prohibit automatic leg selection, complete gait planning, compensating steps,
and whole-body balance.

### Safety supervisor

Safety is a cross-cutting, mandatory boundary immediately before physical
actuation. Every actuator frame passes through it. It owns:

- `/OE` authorization;
- BLE freshness and failsafe;
- FSM/strategy fault handling;
- pulse and frame safety checks;
- bounded retry failure handling;
- forced safe stop and null-actuator fallback.

Runtime safety I/O is serialized separately from safety-state bookkeeping. All
normal PCA9685 and SPI calls return within their configured timeout. If an SPI
transaction times out after entering the ESP-IDF queue, its descriptor and
DMA copy remain owned by the transport context; later calls return
`NOT_READY` until the driver reclaims it, so a late DMA completion cannot
touch stack storage and no caller waits forever for reclamation. ST7789
deinitialization also retains the driver object and bus when that reclaim is
not yet complete; a later deinit call must finish ownership release.
Output-enable and safe-stop failures are returned to the gate and latch an
actuator fault, preventing a later automatic re-arm. The PCA9685 adapter also
enforces the reviewed per-channel hard pulse envelope, and rejects a
configuration that attempts to widen it. Safe-stop independently attempts
`/OE` high, the calibrated rest frame, and SLEEP; an unconfirmed operation
remains pending for a bounded watchdog retry. Revocation paths serialize with
actuator I/O and invalidate an authorization epoch before a revoked frame can
commit. All four configured contact inputs are explicit arm prerequisites for
the normal four-leg runtime.

No UART, BLE, voice, display, or game module may lower `/OE` or call a device
driver directly. The arm predicate is isolated in the pure-C
`safety_policy` component and host-tested for every prerequisite; the runtime
gate still owns `/OE` ordering and safe-stop side effects.

## Dependency rules

1. Pure control and protocol cores compile without ESP-IDF or FreeRTOS.
2. Drivers depend downward on HAL/BSP, never upward on application code.
3. Services depend on driver capabilities, not on private device registers.
4. Application depends on services and control interfaces, never on devices.
5. The game session is an observer and input/session owner; Ghost, scoring,
   feedback, and replay are read-only with respect to actuator output.
6. Physical leg ordering is normalized to `FL/FR/RL/RR` at the public
   interface. Servo-zero diagnostic ordering remains inside its adapter.

## Migration rule

Each migration slice must preserve the existing observable behavior and add a
host test before enabling a new runtime path. The default runtime remains
`BLE_ONLY`; no migration slice authorizes physical motion, flashing, or power
changes. The current host gate covers the pure HAL fakes and the strategy
seam. Independent ESP-IDF v5.5.5 default and landscape HUD builds completed
for `esp32s3`; hardware movement and flash remain separate acceptance gates.
