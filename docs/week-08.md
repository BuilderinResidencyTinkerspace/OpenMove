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

## Next

Measure full-square travel repeatedly from the real `a1`. Only then does it
make sense to ask the mechanism to carry a piece instead of merely pointing at
one.
