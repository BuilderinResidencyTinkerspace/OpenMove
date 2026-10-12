# Week 9 — Adding Replay, Then Figuring Out How to Stop It

**Date:** 20 September 2026

I added a way to give the desktop controller a PGN and have it work through the
moves for both colours. After the last move, it waits ten seconds, sends the
pieces back in reverse order, and can repeat with the loop toggle enabled.

That is the software sequence. I have not run a physical PGN replay yet.

## Playing the moves backwards

The [GUI](../code/openmove_gui.py) reads the mainline with `python-chess` and
checks the moves before starting. It accepts the standard starting position
and rejects captures, castling, promotions, and custom-FEN starts. En passant
is a capture, so that is rejected too.

Those restrictions come from the mechanism. I do not have validated capture
removal, piece replacement, or a complete recovery process for those actions.
Checking the whole sequence first avoids reaching an unsupported move halfway
through a replay.

The GUI asks for fresh controller status, sends one move, and waits for the
response before updating its displayed board and sending the next.

For example, a sequence of `e2e4` followed by `e7e5` would return with
`RETURN e5e7`, then `RETURN e4e2`. This is an example of the command order,
not a hardware test. `RETURN` is separate from a normal chess move because
pawns going backwards would understandably upset the rules checker.

The current firmware also has an experimental knight planner that temporarily
moves blockers. A PGN passing the parser does not establish that those extra
movements will clear the neighbouring pieces.

## The GUI could remember the wrong thing

The follow-up work was about what happens after a failure. The GUI keeps a
copy of the controller's status. If the controller loses its position but the
GUI still thinks it is homed, that old status can make the next action look
ready to run.

Firmware 2.7, still using protocol 7, reports changes to home and board trust.
The GUI refreshes status after a failure or abort, before starting playback,
and before repeating the loop. It also ignores completion messages when no
command is pending and blocks ordinary keyboard controls during replay.

There is a check on the ten-second restore timer too. If I abort a replay,
its old timer must not start returning pieces later. An Abort button should
not come with a surprise sequel.

If something fails, the pieces stay wherever the failed sequence left them.
I have to inspect and restore the physical board, put the carriage at the real
a1, then use Set Home and Reset Board. Those buttons record the setup; they
do not physically restore it.

## What was checked

Firmware 2.6 / protocol 7 has a recorded Uno compile and upload to
`/dev/ttyUSB0`, using 19,146 bytes of flash and 858 bytes of RAM. A read-only
status check confirmed the version, both-side automation, and a1 mapping.
The upload cleared home and board confirmation. No movement was commanded.

The GUI recovery changes were checked with a scripted fake controller, and
2.7 has a recorded host syntax check. There is a conflict in the upload notes:
the earlier weekly entry says 2.7 was AVR-compiled at 19,226 bytes of flash and
858 bytes of RAM and uploaded, while project memory says it was not. I have
kept both records here; the installed 2.7 version needs confirmation.

Neither record includes a physical replay or pickup test. The GUI can follow
successful controller replies, but without piece sensing it cannot see a missed
pickup or a piece left behind.

The replay code is there now. The next useful work is back on the mechanism:
repeat a full-square movement, establish reliable homing, then pick up and move
one piece cleanly. I need that result before this becomes a convincing demo.
