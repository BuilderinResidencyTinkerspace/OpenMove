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
- An experimental cloud Stockfish API exists in `stockfiash/` and is deployed on
  the `az-vm` SSH target under `/home/mishal/openmove-stockfiash`. The container
  is restart-enabled and bound to `127.0.0.1:8080`; it is not publicly reachable
  and is not connected to the physical board.
- An experimental automatic smoke-test sketch exists in
  `code/hardware_smoke_test/`. It contains the exact user-supplied test: both
  motors pulse simultaneously at about 50 steps per second and the servo moves
  between 30 and 120 degrees. It compiled for Arduino Uno and was uploaded via
  `/dev/ttyUSB0`. The user physically ran this uploaded test and reported that it
  works correctly.
- Confirmed diagnostic hardware uses two DRV8825 drivers. Confirmed CNC Shield
  signals are X STEP/DIR D2/D5, Y STEP/DIR D3/D6, shared active-low ENABLE D8,
  and MG90S servo signal D11 through the shield's Z+ header.
- Experimental chess-controller firmware version 0.9 automates white moves only;
  black moves are made physically by the human and then entered to update the
  internal board without carriage motion. Automated knight moves are disabled
  until square-boundary clearance is physically validated. It compiled for Uno
  and was uploaded to `/dev/ttyUSB0` on 2026-09-18. Piece routing remains
  physically unvalidated.
- The user reports both DRV8825 VREF settings are approximately 0.65 V. The
  current-sense resistor markings are not identified, so the resulting limits
  remain unknown. If a carrier is R100, 0.65 V corresponds to about 1.3 A/phase;
  this must not be assumed for clone carriers.
- On 2026-09-19 the origin was initially labelled h1, but physical GOTO tests
  proved that label wrong: `GOTO g1` reached g8 and `GOTO h2` reached h7. Files
  were correct and ranks were exactly mirrored, proving the actual zero location
  is h8. Experimental firmware 2.2 therefore uses h8 as `(0, 0)`, with local X
  increasing toward a-file and local Y increasing toward rank 1. It also bumps
  the serial protocol to version 3 so the GUI rejects the incompatible h1 build.
  Firmware 2.2 compiled, was uploaded to `/dev/ttyUSB0`, reported protocol 3 and
  the h8 coordinate convention, and passed its no-motion self-test. Physical
  motion regression testing remains pending.
- A subsequent protocol-3 test disproved the h8 conclusion: with both axes
  mirrored, `GOTO e2` reached d7 and `GOTO h8` returned to physical a1. This is
  an exact 180-degree transform, establishing a1 as the actual machine origin.
  Firmware 2.3/protocol 4 restores direct file/rank coordinates from a1. It has
  compiled, was uploaded to `/dev/ttyUSB0`, reported the direct a1 coordinate
  convention, and passed its no-motion self-test. Physical regression testing
  remains pending.
- On 2026-09-19 the current working-tree `code/openmove_chess/openmove_chess.ino`
  compiled for Arduino Uno and was uploaded to `/dev/ttyUSB0`. A post-upload
  serial check reported `OpenMove chess motion controller 1.2`, white-only
  automation, and the manual a1-home prompt. No physical motion test was run.
- After the user reported that a false home near h1 made motion appear usable
  while piece colours were reversed, firmware 2.4/protocol 5 made the intended
  mapping explicit: real a1 is `(0, 0)`, positive X is a-to-h, positive Y is
  rank 1-to-8, and white occupies ranks 1-2. The GUI now refuses chess motion
  unless all mapping fields match. Firmware and Python checks pass. Upload was
  blocked because the running GUI held `/dev/ttyUSB0`; physical validation at
  the real a1 remains pending.
- The laptop display is 1366x768 at scale 1 under Niri; the GUI is tiled at
  approximately 800x628. The fixed 1180x760 startup size was too tall. The GUI
  now sizes from available screen geometry, uses tighter spacing, preserves up
  to 2000 log lines, and shows a larger usable scrolling log area. Firmware
  2.4/protocol 5 was then uploaded successfully and its non-motion status output
  confirmed the a1/a-to-h/1-to-8/white-ranks-1-2 contract. Physical motion is
  still unvalidated.
- The user confirmed physical a1 is the bottom-left black square from White's
  viewpoint, White is displayed at the bottom, adjacent square centres are
  44 mm apart, Y+ physically moves a1-to-a2, and the prior X+ moved b1-to-a1.
  They explicitly approved reusing the experimental H-bot transform and
  40 steps/mm calibration. Firmware 2.5/protocol 6 therefore reverses logical X
  only and uses a 44 mm pitch. It compiled, uploaded to `/dev/ttyUSB0`, and a
  no-motion status query confirmed protocol 6, `square_mm=44.00`, and
  `reverse_logical_x=1`. The user then physically confirmed the corrected X+
  jog works as intended. Small positive X and Y direction tests now match the
  a1-to-b1 and a1-to-a2 conventions; full-square accuracy remains unvalidated.

## Unknown or unvalidated

- NEMA 17 and DRV8825 operation under representative mechanical load. An earlier
  fast test caused one motor to hum and the Y driver was reported hotter than X;
  the coordinated approximately 50-step-per-second test works when both motors
  are pulsed simultaneously. Individual-motor behavior, driver VREFs, and the
  previously reported temperature difference remain unverified.
- XY mechanics, accuracy, belt tension, rod rigidity, homing, and calibration
- Permanent-magnet pickup/release mechanism, piece movement, and capture handling
- Hall-sensor selection and 64-sensor architecture
- Public DNS/TLS ingress, host-controller implementation, device authentication,
  network reliability, engine strength/latency targets, and end-to-end move safety

## Documentation rule

Use `docs/week-00.md` for project reporting. Record actual work, blockers, decisions, experiments, and links there; do not create separate meeting, experiment, decision, or BOM documentation sections.
