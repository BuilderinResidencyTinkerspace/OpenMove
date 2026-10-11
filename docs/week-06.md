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

## Three translations hiding inside one square name

A request such as `GOTO e4` passes through several assumptions before anything
moves:

```text
chess square          board distance          motor targets
    e4        →       X/Y in mm       →      A/B step counts
                      origin + pitch         belt transform + scale
```

The square name depends on the board orientation. The distance depends on the
measured square pitch and where zero was assigned. The motor targets depend on
the belt arrangement, motor directions, and steps-per-millimetre calibration.
A mistake in any one of those layers can produce a believable status message
and a carriage sitting somewhere deeply unhelpful.

For the experimental H-bot transform, both motors contribute to XY movement.
Changing one motor direction is therefore not interchangeable with reversing
one logical board axis. That distinction became important in
[Week 8's physical mapping tests](week-08.md).

## Why the host needs more than a version number

The host needs to know what the firmware means by a coordinate. An origin
label, file direction, rank direction, and piece orientation form a contract
between the two programs. Later revisions expose those fields in status and
change the protocol when the interpretation changes.

A host that assumes a1 while the firmware assumes a different corner can still
send perfectly valid text. Syntax checking will happily approve the wrong
journey. This is why the desktop interface checks the reported mapping before
allowing chess motion.

## A reported position is still an estimate

The firmware counts commanded motor steps. Without independent position
feedback, its stored coordinates cannot prove that the mechanism followed
those steps. A missed step, slipping belt, or manually displaced carriage can
separate the internal position from the real one.

This is also why configured travel bounds need physical validation. A number
in the sketch can reject a request outside a rectangle; it cannot establish
that the assembled machine has that much unobstructed travel.

The proposed validation sequence is direction first, then scale, then repeated
returns to the same point. Record commanded distance, measured distance, and
return error for each axis. Until those measurements exist, changing more
constants only makes the uncertainty harder to track.

## Next

Run the smallest physical tests first: set the real `a1`, jog positive X and Y,
and compare a commanded square with the board. One measured fact beats three
pages of coordinate folklore.
