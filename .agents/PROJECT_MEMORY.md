# OpenMove project memory

## Phase and objective

Early prototype — mechanical development. The immediate objective is to build and validate an XY motion system that can accurately move a magnetic pickup mechanism between chessboard squares.

## Current prototype direction

- Arduino Uno with CNC Shield V3
- 2 × NEMA 17, 2 × DRV8825, GT2 belt drive
- 2 × 4 mm × 300 mm stainless-steel rods — experimental prototype choice
- Servo-operated permanent magnet — mechanism not finalized
- 12 V motion power target

## Confirmed facts

- Arduino handles low-level motion and hardware control.
- A host computer will later handle chess logic, Stockfish, UI, and online integrations over USB serial.
- Position detection is planned as an 8 × 8 Hall-effect sensor grid, but sensor, PCB, and scanning architecture are unvalidated.
- Arduino Uno and servo are already available.

## Unknown or unvalidated

- NEMA 17 and DRV8825 operation with this prototype
- XY mechanics, accuracy, belt tension, rod rigidity, homing, and calibration
- Permanent-magnet pickup/release mechanism, piece movement, and capture handling
- Hall-sensor selection and 64-sensor architecture

## Documentation rule

Use only `docs/week-01.md` through `docs/week-09.md` for project reporting. Record actual work, blockers, decisions, experiments, and links in the relevant week; do not create separate meeting, experiment, decision, or BOM documentation sections.
