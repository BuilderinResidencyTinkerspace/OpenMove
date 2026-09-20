# OpenMove

OpenMove is my attempt to build an automatic chessboard that can move pieces
from underneath the board. The idea is straightforward. The implementation is
not. Magnets, belts, loose tolerances, and chess pieces have opinions.

Right now this is an **experimental XY-motion prototype**, not an autonomous
chessboard. The current job is proving that the carriage can move repeatably
between squares. Everything above that layer—pickup, sensing, engine play, and
a complete game—waits its turn.

## What exists today

| Part | Current direction | Reality check |
| --- | --- | --- |
| Controller | Arduino Uno + CNC Shield V3 | Firmware 2.7 / protocol 7 uploaded |
| Motion | 2 × NEMA 17, 2 × DRV8825, GT2 belts | Experimental; full travel unvalidated |
| Linear support | 2 × 4 mm × 300 mm rods | Experimental |
| Pickup | MG90S servo + permanent magnet | Mechanism exists; piece pickup unvalidated |
| Mapping | `a1` origin, X a→h, Y 1→8, 44 mm pitch | Jog directions confirmed; accuracy unmeasured |
| Desktop control | PySide6 GUI and terminal UI | Experimental, with firmware trust checks |
| Chess replay | Non-capture standard-start PGNs only | Software path only; no physical replay yet |
| Sensing | 8 × 8 Hall grid is the idea | Not built or validated |
| Engine | Containerized Stockfish API | Separate from the physical board |

The board will not reliably play a game yet. Captures, castling, en passant,
promotion, automatic homing, full-square repeatability, and collision clearance
are all unfinished. Calling it “AI chessboard complete” now would be LinkedIn
fiction with stepper motors.

## The split that keeps this project sane

```text
physical board ──> host controller ──> chess engine
      ^                   |
      |                   v
XY carriage <──── Arduino low-level motion
```

The Arduino handles pins, steps, timing, and the magnet actuator. The host is
responsible for chess rules, game state, UI, and any future online work. Engine
output never gets to move hardware by itself; the host must confirm it still
matches the board state first.

## Hardware direction

- Arduino Uno and CNC Shield V3
- Two NEMA 17 motors and two DRV8825 drivers
- GT2 belt drive and two 4 mm × 300 mm stainless-steel rods
- 12 V motion-power target
- MG90S servo driving a 3D-printed permanent-magnet mechanism

These are prototype decisions, not a shopping list carved into stone. The rod
choice, XY kinematics, pickup design, sensing architecture, host language, and
serial protocol can change when tests give a good reason.

## Running the experimental tools

### Firmware

The sketch is [`code/openmove_chess/openmove_chess.ino`](code/openmove_chess/openmove_chess.ino).
Compile/upload it for an Arduino Uno with the Arduino IDE or Arduino CLI.

```bash
arduino-cli compile --fqbn arduino:avr:uno code/openmove_chess
arduino-cli upload --port /dev/ttyUSB0 --fqbn arduino:avr:uno code/openmove_chess
```

An upload clears manual home and board confirmation. Set the carriage at the
real `a1` centre, then home and reset/confirm the board before requesting any
movement. Start with small jogs; do not jump straight to a PGN because you are
feeling brave.

### Desktop GUI

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r code/requirements-gui.txt
python code/openmove_gui.py --port /dev/ttyUSB0
```

The GUI is deliberately strict about the controller mapping and trust state. If
home or board confirmation is lost, stop, re-home, and reset the board. A
failed move or replay does not put physical pieces back for you.

For the lower-level coordinated motor/servo test, read
[`code/hardware_smoke_test/README.md`](code/hardware_smoke_test/README.md)
before uploading it. In particular: the servo needs suitable regulated 5 V
power with common ground, never the 12 V motor supply.

## Project map

- [`docs/`](docs/index.md) — the build journal, Weeks 0–9
- [`code/`](code/README.md) — firmware, GUI, terminal UI, and smoke test
- [`cad/`](cad/README.md) — mechanical files and reference assets
- [`stockfiash/`](stockfiash/README.md) — the deliberately separate Stockfish API

## Next useful test

Validate repeatable full-square XY travel from `a1`, then add homing and test
the magnet moving exactly one piece. That is the next honest milestone. More
chess logic before that is just avoiding the hard mechanical work with extra
syntax.
