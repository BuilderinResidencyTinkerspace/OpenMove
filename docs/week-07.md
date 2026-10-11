# Week 7 — White-side Motion Firmware

**Date:** 18 September 2026

The chess-controller firmware now automates white pieces only. After a human
physically moves a black piece, entering that coordinate move validates it and
updates the firmware's internal board without moving the carriage. White moves
continue to use the existing piece-aware validation and mechanical routing.

That split was intentional. It let the controller exercise move validation
without pretending the board could safely route every piece for both sides.
Less impressive on a demo video, much less likely to create an expensive new
problem.

Automated knight moves are rejected for now. The square-boundary lane route
cannot be called collision-free until it is measured against the largest piece
base and tested on the physical board.

Firmware version 0.9 compiled for Arduino Uno using 12,880 bytes of flash and
527 bytes of SRAM, then uploaded successfully to `/dev/ttyUSB0`. It accepts
legal human-performed black captures as state-only updates, rejects unvalidated
promotion for both colours, rejects king capture, invalidates state after a
rejected black entry, and guards step timing against unsigned underflow.

No HOME or motion command was issued during the upload. Physical movement,
clearance, and accuracy are still unvalidated for this build. Captures,
castling, en passant, and physical promotion stay blocked until their
mechanisms are proven.

## What a turn meant in this build

This entry describes firmware 0.9 on 18 September. Later versions change the
behaviour, so white-only automation should not be read as the current feature
set. The [Week 9 replay work](week-09.md) adds both-side automation.

In this version, a White move requested physical execution. A Black move was
entered after the human had already performed it. The controller checked the
entry and updated its internal board without repeating that movement.

That creates a slightly awkward failure case: if a human physically makes a
Black move and then enters an invalid move, rejecting the text cannot undo
what happened on the board. Invalidating the internal board state is the
honest response. Otherwise, the next White move would start from a position
the controller has no reason to trust.

## A knight is where the drawing gets suspicious

On a chess diagram, a knight jumps. Under a real board, the magnet still needs
a continuous route between the source and destination. Drawing an L-shaped
line does not establish that the moving piece will clear its neighbours.

This build rejects automated knight movement while the proposed lane clearance
remains unmeasured. The relevant dimensions include the largest piece base,
the available space between pieces, and the error in carriage positioning.
A mathematically legal move can fail every one of those physical checks.

The current source later includes an experimental planner that temporarily
relocates blockers. Its existence does not retroactively validate this week's
route, and its physical behaviour remains unproven.

## What the build result actually bought me

The recorded compile and upload show that the 0.9 sketch fit the Uno and could
be installed. The flash and SRAM figures above are build figures, not a motion
benchmark or a measurement of runtime memory headroom.

The useful next step was to establish the board's coordinates physically.
Before asking the carriage to carry White's opening move, I needed to settle
which direction White's side actually was. The next entry explains why that
turned out to deserve an entire week of documentation.
