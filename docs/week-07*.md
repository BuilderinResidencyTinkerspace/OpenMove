# Week 7 - White-side motion firmware

**Date:** 18 September 2026

The chess-controller firmware now automates only white pieces. After the human
physically moves a black piece, entering that coordinate move validates it and
updates the firmware's internal board without moving the carriage. White moves
continue to use the existing piece-aware validation and mechanical routing.
Automated knight moves are now rejected because the square-boundary lane route
cannot be called collision-free until it is measured against the largest piece
base and tested on the physical board.

Firmware version 0.9 compiles for Arduino Uno using 12,880 bytes of flash and
527 bytes of SRAM. It also accepts legal human-performed black captures as
state-only updates, rejects unvalidated promotion for both colours, rejects king
capture, invalidates state after a rejected black entry, and guards step timing
against unsigned underflow.

The v0.9 build was uploaded successfully to `/dev/ttyUSB0`. No HOME or motion
command was issued during upload.

Physical movement, clearance, and accuracy are still unvalidated for this
firmware build. Captures, castling, en passant, and physical promotion remain
blocked until their mechanisms are validated.
