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
