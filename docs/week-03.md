# Week 3 — Making the Magnet Go Up and Down

**Goal this week:** Build the first physical part of the pickup system: a way for the magnet to move vertically underneath the board.

The XY mechanism can move a magnet around the board later. But a magnet that is permanently pressed against the board is not really a pickup mechanism. It is just a magnet with commitment issues.

So this week, I worked on the up-and-down movement.

## The linear servo idea

The plan was to use a servo with 3D-printed parts that convert its rotation into linear movement.

When the servo moves one way, the magnet comes closer to the underside of the chessboard and can pick up a magnetic chess piece. When it moves back, the magnet retracts and releases the piece.

It is a small mechanism, but it is doing an important job. The XY system may know exactly where to go, but it still needs a way to actually grab and release a piece once it gets there.

## Printing the first parts

I 3D printed the linear-servo parts for this mechanism.

This was the first time OpenMove started getting a physical pickup system instead of just motors, wires, and increasingly ambitious diagrams.

The parts were meant to hold the magnet and let the servo move it vertically. The design still needed real testing with the board, the magnet, and actual chess pieces, but printing the mechanism was the step that made those tests possible.

## What is still unknown

A printed mechanism is not automatically a working mechanism. The important questions were still waiting:

* Does the servo have enough force to move the magnet reliably?
* Does the magnet pick up a piece through the board?
* Can it release the piece cleanly?
* Does the mechanism bind, flex, or collide with the XY carriage?
* How much vertical travel is actually needed?

The answer to all of those was currently: we will find out when the hardware stops being theoretical.