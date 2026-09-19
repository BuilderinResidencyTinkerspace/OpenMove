# Week 0 — How OpenMove Started

**Date:** 12 July

## A chessboard was apparently not dramatic enough

OpenMove started with a random YouTube video I cannot find anymore.

Somewhere between that and the magical chessboard from *Harry Potter*, I got stuck on a question: could I make a real chessboard that understood what was happening on it, thought about the next move, and physically moved a piece back?

Not a finished product. Not even a particularly sensible first project idea. Just a very specific kind of curiosity that refused to leave.

I had been into open source software for a while, so the name **OpenMove** felt right

## Getting into BIR

I was sitting in a regular, boring college lab when I started randomly trying to join different TinkerHub groups.

Somehow, even after registrations had closed, I got into the BIR WhatsApp group. Shann, one of the admins, contacted me, heard the project idea, and let me join BIR properly.

That was how OpenMove accidentally became a Builder-in-Residence project.

At that point, OpenMove was still mostly a concept. There was no assembled board, no tested mechanism, and definitely no tiny wizard living under the chessboard moving pieces around.

But it was enough of an idea to explore properly.

## The first technical sketch

The first version in my head had three main parts:

* An 8×8 grid of Hall-effect sensors — one for each square — to detect piece positions.
* A local Stockfish chess engine to calculate a response.
* An automated mechanism underneath the board to move chess pieces.

The practical details were not simple at all.

How would the board distinguish pieces reliably? How would a mechanism move across all 64 squares without getting stuck, losing position, or dragging pieces it was not supposed to touch? Could magnets move a piece through the board cleanly? Could the electronics, mechanics, and chess logic be kept separate enough that debugging one did not destroy the others?

At this stage, those were questions—not solved engineering decisions.

## Why I wanted to explore it

OpenMove was interesting because it sat right at the intersection of things I wanted to learn: electronics, mechanical design, embedded control, chess logic, and software that has consequences in the physical world.

A chess engine returning a move in software is normal. A board physically carrying that move out is where it gets complicated.

That seemed like a good reason to try it.

## What came next

The next step was to turn the vague sketch into smaller experiments: decide how the XY mechanism might work, identify the controller and motion hardware, and validate movement before pretending the board could play chess.

Because a brilliant chess engine is not very useful if the board responds by launching a knight into another dimension.

## Experimental corner-origin update — 19 September 2026

The manual XY origin was changed from the centre of `a1` to the centre of `h1`.
Firmware 2.1 mirrors the X coordinate while preserving normal chess file names:
`h1` is `(0, 0)`, local positive X points toward the a-file, and `a8` is
`(306.25, 306.25)` mm. The firmware and host prompts were updated and the Uno
build succeeds. Firmware 2.0 was uploaded to `/dev/ttyUSB0`, then `HOME` was
accepted at h1 and `status` reported `homed=1,x=0.00,y=0.00`. Physical movement
direction validation remains pending. A reversible 1 mm test completed X+, X-,
Y+, and Y- jogs without a controller error or reset and returned the reported
position to `(0.00, 0.00)`; the observed physical directions still need user
confirmation.

The firmware and desktop GUI were then audited together. Firmware 2.1 now
reports serial protocol version 2 and its `h1` home convention. The GUI waits
for startup before requesting status, rejects incompatible firmware/home
coordinates, refreshes status after manual hardware commands, and invalidates
GUI board synchronization when firmware reports an untrusted board or a
physical-state failure. Firmware 2.1 compiles; upload and physical regression
testing were next. Firmware 2.1 was uploaded to `/dev/ttyUSB0`; live `status`
reported protocol 2 and `home_square=h1`, and `SELFTEST` passed without motion.
The upload correctly cleared home and board confirmation. Physical motion
regression testing remains pending.

Follow-up physical GOTO tests exposed that the h1 label was wrong: `GOTO g1`
reached the black knight on g8, while `GOTO h2` reached the black pawn on h7.
The file axis was correct and the rank axis was exactly mirrored. The actual
manual origin is therefore h8. Firmware 2.2 changes the mapping to `h8 = (0, 0)`,
with positive X toward a-file and positive Y toward rank 1. The serial protocol
was bumped to version 3 so the GUI cannot connect its motion controls to the
incompatible h1 mapping. Firmware 2.2 was uploaded to `/dev/ttyUSB0`; live status
reported protocol 3, `home_square=h8`, and positive Y toward rank 1, and the
no-motion self-test passed. Physical regression testing remains pending.

The h8 conclusion was then falsified by a stronger two-axis test. Under the
mirrored build, `GOTO e2` reached d7 and `GOTO h8` returned to physical a1—an
exact 180-degree mapping. The actual machine origin is a1. Firmware 2.3 restores
direct coordinates (`a1 = (0, 0)`, positive X toward h-file, positive Y toward
rank 8) and bumps the protocol to version 4. Firmware 2.3 was uploaded to
`/dev/ttyUSB0`; live status reported the direct a1 coordinate convention and
the no-motion self-test passed. Physical regression testing remains pending.

Later on 19 September, the current working-tree chess-controller sketch was
compiled for Arduino Uno and uploaded to `/dev/ttyUSB0`. The upload completed
successfully, and a non-motion serial check showed the startup banner
`OpenMove chess motion controller 1.2`, white-only automation mode, and the
manual a1-home prompt. No physical movement was commanded or validated.

The next mapping revision removes the false-origin workaround. Firmware
2.4/protocol 5 explicitly defines real `a1` as `(0, 0)`, positive X from a-file
to h-file, positive Y from rank 1 to rank 8, and white pieces on ranks 1-2. The
GUI checks all five mapping fields and keeps chess movement disabled if the
connected firmware disagrees. The Uno build and Python syntax checks passed.
Upload was blocked because the running GUI held `/dev/ttyUSB0`; physical
validation from the real a1 is still pending.

The laptop display was measured as 1366x768 at 100% scale under Niri, where the
GUI receives an approximately 800x628 tile. Its old fixed 1180x760 startup size
was too tall. The layout now uses available screen geometry, tighter controls,
a visible scrolling log pane, and a 2000-line log history. Firmware 2.4/protocol
5 was then uploaded successfully. A non-motion status query confirmed
`home_square=a1`, `x_axis=a-to-h`, `y_axis=1-to-8`, and `white_ranks=1-2`.
Physical motion from the real a1 remains untested.

The coordinate mapping was then rebuilt from new physical observations. From
White's viewpoint, a1 is the bottom-left black square; Y+ moves from a1 toward
a2, while the previous X+ moved in the wrong direction from b1 toward a1. The
measured adjacent-square centre spacing is 44 mm. With explicit user approval,
the experimental H-bot transform and 40 steps/mm calibration were retained.
Firmware 2.5/protocol 6 reverses logical X only and uses the 44 mm pitch. It
compiled and uploaded successfully, and a non-motion status query confirmed
protocol 6, `square_mm=44.00`, and `reverse_logical_x=1`. Physical regression
testing remained pending. The user subsequently confirmed that the corrected
positive-X jog works as intended. Small positive X and Y direction tests now
match a1-to-b1 and a1-to-a2; full-square positioning accuracy is still
unvalidated.
