# Week 6 — A Coordinate System Is Not a Vibe

**Goal this week:** Make the firmware's idea of a chess square explicit enough
to test it against the actual machine.

Writing `goto e4` looks cute until the carriage heads toward a different corner
than the one in your head. The early controller revisions made that risk very
obvious: motor direction, H-bot transforms, manual home, and chess notation
were all tightly coupled.

## The useful part: fewer hidden assumptions

The motion firmware was tightened around a manual-home workflow and explicit
status reporting. The controller can report its coordinate contract, and the
host tools can refuse to issue chess motion when the connected firmware does
not match the expected mapping.

That is not glamorous work. It is also the difference between a mapping mistake
being a visible error and it becoming a very confident rook relocation.

## Still experimental

At this point, the origin label, positive-axis directions, steps-per-millimetre
calibration, and square pitch were not final. The firmware could compile and be
uploaded, but compilation only proves that C++ survived the compiler. It says
nothing about where a magnet ends up in real space.

The 4 mm rods, belt geometry, H-bot transform, and pickup mechanism also
remained prototype choices. Nothing here promoted them to final hardware.

## Next

Run the smallest physical tests first: set the real `a1`, jog positive X and Y,
and compare a commanded square with the board. One measured fact beats three
pages of coordinate folklore.
