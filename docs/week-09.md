# Week 9 — Replaying a Game Without Lying About the Hard Parts

**Date:** 20 September 2026

The desktop controller and firmware gained an experimental PGN replay path.
Given a mainline PGN from the standard starting position, the GUI can send
non-capturing moves for both colours, wait 10 seconds after the last move, and
send reverse `RETURN` moves to restore the standard position. The loop toggle
can repeat that sequence.

This is a controlled replay experiment, not autonomous chess. The parser
rejects captures, castling, en passant, promotions, and custom-FEN games. Those
need pickup, release, removal storage, collision clearance, and recovery paths
that have not been physically validated. Calling that “feature incomplete” is
too polite; it would be physically dishonest.

## Trust is part of motion

The follow-up firmware 2.7 keeps protocol 7 and improves the failure boundary.
If an emergency stop, failed move, failed return, or DISABLE command loses home
or board trust, the controller reports that change. The GUI refreshes its state
before replay and after failure instead of continuing from a stale cache.

That matters because a failed replay does not restore the physical board. The
operator must set home and reset the board before new movement. There is no
clever software shortcut around a carriage that no longer knows where it is.

Firmware 2.7 compiled for an Arduino Uno using 19,226 bytes of flash and 858
bytes of RAM, and was uploaded to `/dev/ttyUSB0`. The GUI replay changes were
checked headlessly against a scripted fake controller. No post-upload axis
motion, physical replay, pickup test, or full game has been run.

## What the replay loop actually does

The [desktop GUI](../code/openmove_gui.py) parses the PGN mainline using
`python-chess`, checks its supported move restrictions before playback, and
requires the standard starting position. It refreshes controller status before
starting, then sends one move at a time. The displayed board advances after a
successful controller response.

```text
parse supported PGN → refresh status → send move → wait for response
                                             ↑            |
                                             +-- next move+

last move → wait 10 seconds → RETURN moves in reverse order
                                      |
                         restore complete → finish or recheck and loop
```

A short example makes the reverse order easier to see. This is an explanation
of the software sequence, not a recorded hardware test:

| Stage | Command sequence |
| --- | --- |
| Forward example | `e2e4`, then `e7e5` |
| Pause | 10 seconds |
| Restore example | `RETURN e5e7`, then `RETURN e4e2` |

Returning a pawn backwards is a restoration operation, which is why `RETURN`
is separate from an ordinary legal chess move. Reversing the order also matters
when later moves depend on spaces opened by earlier ones.

The GUI can track successful commands. Without piece sensing, it cannot verify
that each physical piece followed them. A successful replay in the interface
is still a software result until the board itself has been observed.

## Why the restrictions belong at the beginning

The parser rejects an unsupported mainline before sending the first move.
Discovering a capture halfway through would leave the operator with a partly
played position and a controller that has run out of supported actions.

| Unsupported action | Missing physical capability or workflow |
| --- | --- |
| Capture, including en passant | Remove and account for another piece |
| Castling | Coordinate and validate a two-piece move |
| Promotion | Replace a pawn and reconcile the replacement |
| Custom starting position | Establish a trusted nonstandard physical setup |

A non-capturing move can still have an unvalidated physical route. The current
firmware's knight planner may temporarily move blockers, which adds movements
beyond the move printed in the PGN. Parser acceptance does not certify that
those routes have adequate clearance.

## The stale-state bug was a hardware problem wearing a GUI costume

The GUI caches the controller's status. If the firmware loses home or board
trust but the cached display still says it is ready, the next action can be
based on yesterday's confidence—or, more realistically, confidence from two
seconds before an abort.

Firmware 2.7 reports trust changes through `info:` fields. The GUI refreshes
status after failures and aborts, checks again before replay, and checks trust
before repeating a loop. Unsolicited completion lines are ignored when no
command is pending, and keyboard controls are gated during playback so a
second interaction cannot casually join the sequence.

The delayed restore callback also checks that it belongs to the active replay.
An old ten-second timer should not wake up after an abort and decide it still
has a job. That is a very small detail until there is a motor attached to it.

## The upload record needs reconciliation

The earlier paragraph in this entry records a 2.7 AVR build of 19,226 bytes
of flash and 858 bytes of RAM, followed by an upload. The repository's agent
memory instead records only a host syntax check and a headless GUI test for
2.7, explicitly saying it was not AVR-compiled or uploaded. These records
conflict. The upload claim is retained as historical documentation, but the
installed 2.7 version should be treated as unconfirmed until that discrepancy
is resolved with a build/upload record or a fresh controller identification.

Firmware 2.6 / protocol 7 has a separate recorded compile, upload, and read-only
status confirmation. Neither version has a recorded physical PGN replay.
A fake controller can exercise response handling and recovery logic; it cannot
report belt slip, a missed pickup, or a collision on the real board.

## Recovery includes physically restoring the board

After a failed or aborted replay, stop the sequence and inspect the physical
setup. Restore the pieces to the standard position, re-establish the carriage
at the real a1 origin, and only then use Set Home and Reset Board to make the
software agree with that setup. Clearing an error message alone does not
restore either position.

The next milestone remains a repeatable square movement, followed by one
reliable pickup, move, and release. There is now plenty of software ready to
request more ambitious movement. The mechanism still gets to decide whether
that ambition is reasonable.

## Next

Stop adding chess layers for a minute and validate the actual XY machine:
repeatable full-square travel, homing, and a magnet moving one piece cleanly.
The software is allowed to wait. Gravity and friction have been waiting longer.
