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
