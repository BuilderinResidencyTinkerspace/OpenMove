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

## Two different things the operator has to confirm

Manual home and board confirmation answer different questions.

`HOME` tells the controller where the carriage is. The operator places it at
the intended origin and the firmware assigns that location zero. It does not
search for a switch or discover the corner by itself. Calling a command HOME
does not quietly install homing hardware.

`RESETBOARD` tells the controller that the pieces match the standard starting
position. It resets the internal representation; it does not move scattered
pieces back into place. If the physical board is wrong, this command can make
the software confidently wrong with it.

That distinction follows the project all the way into replay recovery. Knowing
where the carriage is and knowing where the pieces are are separate jobs.

## A command needs an ending

A terminal interface also needs to distinguish “I sent something” from “the
controller finished it.” The current serial implementation uses responses such
as `busy:`, `ok:`, and `error:`, plus `status:` and `info:` fields for state.
These are described here as a reading guide to the current source, rather than
an exact transcript of this week's historical build.

| Command family | What it means | What it cannot establish |
| --- | --- | --- |
| `status` | Report the controller's stored state | Independent physical position |
| `HOME` | Accept the operator's origin | Automatic homing accuracy |
| `RESETBOARD` | Declare a standard physical setup | Whether pieces really match it |
| `GOTO` / jog | Request carriage travel | Successful piece transport |
| Actuator endpoints | Command the magnet mechanism | Measured pickup or release |
| Abort | Stop the running sequence | Undo movement already performed |

The current implementation can be inspected in the
[firmware](../code/openmove_chess/openmove_chess.ino),
[terminal UI](../code/openmove_tui.py), and
[desktop GUI](../code/openmove_gui.py).

## The architecture still has some prototype baggage

The sketch contains an internal chess board and move checks. That is useful
for these experiments, but it means the present implementation goes beyond the
intended long-term split of responsibilities. The host already uses chess
software in the desktop path; the eventual architecture should keep full game
rules and reconciliation on that side.

Documenting the intended boundary is useful. Pretending the code already fits
it perfectly would be creative writing with a USB cable.

## Next

Use small manual jogs to establish a real origin and axis directions, then
measure one square before allowing the firmware to speak confidently about
`a1`, `h8`, or anything in between.
