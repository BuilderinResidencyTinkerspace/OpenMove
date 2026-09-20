# Week 5 — Teaching the Board to Say “No”

**Goal this week:** Give the motion prototype a small serial controller without
pretending that it was ready to play chess.

The motors had already passed a very small bench test. That was useful, but
“both motors moved” is not a coordinate system, a safety boundary, or a way to
recover when the carriage gets lost. This week was about putting a boring but
necessary layer between a terminal command and the hardware.

## The controller got a vocabulary

The experimental Arduino sketch in
[`code/openmove_chess/`](../code/openmove_chess/) gained commands for manual
home, status, jogging, square moves, actuator endpoints, board reset, and an
emergency stop. It also keeps a small internal chess position so it can reject
obviously illegal requests instead of letting every typo become motor movement.

That does **not** make it a chessboard yet. It is a motion-controller prototype
wearing a chess notation trench coat.

The important boundary stayed the same: the Uno owns pins, timing, and the
actuator. A host computer will eventually own game rules, engine calls, and
anything involving the internet.

## A terminal UI, because serial monitors get old fast

[`code/openmove_tui.py`](../code/openmove_tui.py) was added as a thin manual
control interface. It can send commands and show the controller response; it
does not bypass the firmware's checks.

The controller starts untrusted after upload. Someone has to set the physical
home position and explicitly reset/confirm the board before chess-style motion
is allowed. That is deliberately annoying. An automatic board should be a
little suspicious of itself before moving pieces around.

## What this did not prove

No physical chess move, square accuracy, clearance, homing repeatability, or
pickup behaviour was validated here. The coordinate calibration and routing in
the firmware remained experimental. Software restrictions are not a substitute
for a carriage that has actually moved under load.

## Next

Use small manual jogs to establish a real origin and axis directions, then
measure one square before allowing the firmware to speak confidently about
`a1`, `h8`, or anything in between.
