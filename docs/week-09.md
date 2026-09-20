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

## Next

Stop adding chess layers for a minute and validate the actual XY machine:
repeatable full-square travel, homing, and a magnet moving one piece cleanly.
The software is allowed to wait. Gravity and friction have been waiting longer.
