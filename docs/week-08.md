# Week 8 — The Board Finally Argued Back

**Date:** 19 September 2026

This week was a coordinate-system reality check. The first manual-origin label
was `h1`. Physical GOTO tests disagreed: `GOTO g1` reached `g8`, and `GOTO h2`
reached `h7`. That ruled out the label. A later two-axis test ruled out the
next theory too: the apparent `h8` mapping was an exact 180-degree transform,
which established the actual machine origin as `a1`.

That is not a cute correction. A wrong mapping can move a real carriage to the
wrong square while software insists it is being helpful. The firmware protocol
was bumped as incompatible coordinate contracts were corrected so the GUI would
not quietly drive a newer board with an older assumption.

## The current contract

The current experimental mapping is:

```text
a1 = (0, 0)
+X = a-file → h-file
+Y = rank 1 → rank 8
white pieces = ranks 1–2
square pitch = 44 mm
```

The 44 mm centre-to-centre pitch came from a physical measurement. The existing
H-bot transform and 40 steps/mm calibration were retained with explicit
approval; they are still experimental, not magically certified by being in a
constant.

Firmware 2.5/protocol 6 compiled and uploaded. Small positive X and Y jogs
were physically confirmed to match `a1 → b1` and `a1 → a2`. That is a real win,
but keep the champagne corked: full-square positioning accuracy, repeatability,
belt tension, rod rigidity, and motion under pickup load are still unmeasured.

## The wrong answers are part of the result

The mapping history is worth preserving because each observation ruled out a
specific assumption:

| Interpretation being tested | Recorded physical observation | What it showed |
| --- | --- | --- |
| Origin labelled h1 | `GOTO g1` reached g8; `GOTO h2` reached h7 | File labels matched, ranks were mirrored |
| Both axes mirrored from an assumed h8 | `GOTO e2` reached d7; `GOTO h8` returned to physical a1 | The interpretation was rotated 180 degrees |
| Direct a1 convention before the X correction | Positive Y went a1→a2; positive X went b1→a1 | Logical X needed reversal |
| Firmware 2.5 / protocol 6 | Corrected positive X jog confirmed | Small positive X/Y directions matched the intended board axes |

The original detailed notes remain in
[Week 0's September update](week-00.md#experimental-corner-origin-update--19-september-2026).
They include intermediate uploads, the unexpected 1.2 startup banner, the
serial-port conflict, and no-motion self-tests. Keeping those details makes it
possible to follow the investigation instead of presenting the final mapping
as something I knew all along. I very clearly did not.

## What 44 mm means in coordinates

With a1 at zero, square centres follow this experimental mapping:

```text
                 +Y, towards rank 8
                  ↑
a8 (0, 308)       |                     h8 (308, 308)
                  |
a2 (0, 44)        |
a1 (0, 0) --------+--------------------→ +X, towards h
                    b1 (44, 0)          h1 (308, 0)
```

There are seven centre-to-centre gaps from a1 to h1: `7 × 44 = 308 mm`.
That is a calculated coordinate span, not measured usable carriage travel.
The listed 300 mm rods therefore need particular attention when evaluating
full-board reach: their nominal length is already shorter than that centre
span, and their role and mounting in the actual mechanism must be checked.
This does not approve replacement rods or settle the mechanical design.

The 40 steps/mm value is still the approved experimental calibration. Correct
direction and correct scale are separate results; one successful positive jog
cannot certify a 44 mm movement.

## The GUI also had to fit on the actual laptop

The laptop display was 1366×768 at scale 1, while Niri gave the GUI a tile of
roughly 800×628. A fixed 1180×760 startup size was an optimistic interpretation
of available space.

The interface was changed to size itself from the available screen geometry,
use tighter spacing, and keep a usable scrolling log with up to 2000 lines.
That matters during a hardware experiment: the controller's explanation should
be visible when it refuses a command. A log pane below the screen is technically
logging, in the same way a notebook in another building is technically nearby.

## The next measurement — Experimental

Start from a physically established a1 with the pickup released. After small
jogs confirm direction and clearance, measure repeated one-square movements
and returns on each axis. Record the actual distances and return offsets,
along with the calibration and mechanical setup used.

Full-board travel should follow a reach and clearance check. No repeatability
tolerance has been validated yet, so this log does not award the mechanism a
pass simply because the endpoint looks close in a photo.

## Next

Measure full-square travel repeatedly from the real `a1`. Only then does it
make sense to ask the mechanism to carry a piece instead of merely pointing at
one.
