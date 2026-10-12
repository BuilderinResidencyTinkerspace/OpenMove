# Week 8 — I Told It g1. It Went to g8.

**Date:** 19 September 2026

I had labelled the manual origin `h1`. Then `GOTO g1` took the carriage to g8,
and `GOTO h2` took it to h7.

The files were right. The ranks were mirrored. At least the mistake was
consistent, which made it a little easier to investigate.

The next interpretation was h8. That did not survive testing either: with
both axes mirrored, `GOTO e2` reached d7, and `GOTO h8` returned to physical
a1. The mapping was rotated 180 degrees. The actual machine origin was a1.

## Getting the directions straight

From White's viewpoint, a1 is the bottom-left black square. Positive X should
move towards b1, and positive Y towards a2. White starts on ranks 1 and 2.

Y was already moving the right way. The previous positive X moved from b1
towards a1, so firmware 2.5 reversed logical X while keeping the experimental
H-bot transform. I explicitly kept the 40 steps/mm calibration for this test.

The measured distance between neighbouring square centres was 44 mm. That
puts b1 at `(44, 0)`, a2 at `(0, 44)`, and h8 at `(308, 308)` when a1 is zero.
Those are the intended coordinates; I still had to measure how accurately the
carriage reached them.

Firmware 2.5 / protocol 6 compiled and uploaded. The corrected positive-X jog
was physically confirmed, and the small positive X and Y tests now matched
a1→b1 and a1→a2. Finally, the direction labels agreed with the board.

## Keeping the interface in sync

The protocol changed as the mapping changed, and the GUI checks the reported
origin and directions before allowing chess movement. Otherwise an older
interface could connect successfully and send commands using the wrong corner.

The detailed [September notes in Week 0](week-00.md#experimental-corner-origin-update--19-september-2026)
keep the intermediate uploads and self-tests, including the unexpected 1.2
startup banner and the upload blocked by the GUI holding the serial port.
There were several revisions between the first guess and the working jogs.

The GUI also needed to fit my laptop. The display is 1366×768, and Niri gave
the window roughly 800×628. Its fixed 1180×760 startup size was too large.
The layout now uses the available screen size, tighter spacing, and a scrolling
log with up to 2000 lines. Being able to read the controller's reply without
fighting the window was a useful improvement.

## Direction is sorted; distance is next

I still needed repeated full-square measurements, return-position checks, and
testing under pickup load. Belt tension and rod rigidity were also unmeasured.
The next experimental test is one square out and back on each axis, recording
actual travel and return error.

There is a reach question to check too. Seven 44 mm gaps span 308 mm, while
the listed rods are 300 mm long. Their mounting and role in the mechanism need
to be checked against the required travel before claiming full-board coverage.
No hardware replacement has been decided.

This week settled the small-jog directions. I am keeping that result separate
from accuracy because there is still a fair amount of board left to reach.
