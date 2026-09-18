# Week 3 — Giving the Magnet a Job

**Goal this week:** Build the first version of the mechanism that lets the magnet pick up and release a chess piece.

Up to this point, the plan had a magnet underneath the board. Cool. Very magnetic. But a magnet sitting at one height cannot really choose whether to hold a piece or let it go.

It needs to move.

That sounds obvious now, but it changed the pickup idea from “put a magnet under the board” into “build a tiny mechanism that can raise and lower a magnet without making the whole board look like it has a second gearbox attached.”

So this week, I worked on the vertical pickup mechanism: a servo-driven, 3D-printed setup that would move the magnet up toward the board to grab a piece, then pull it back down to release it.

<img width="449" height="420" alt="image" src="https://github.com/user-attachments/assets/ea00feb4-d51f-4d8f-b29f-830684da3804" />



The goal was simple: when OpenMove wants to move a piece, the magnet goes up, grabs it from below, and the XY system carries it to the next square. When it reaches the destination, the magnet comes down and hopefully lets go instead of deciding that the piece belongs to it now.

## Turning servo rotation into useful movement

A servo rotates. The magnet needs to move up and down.

Those are not the same thing, which meant I needed a small mechanism in between. The idea was to use 3D-printed linear-servo parts to convert the servo’s rotary movement into vertical travel for the magnet.

This is one of those parts that looks small in the CAD model and somehow becomes the entire project when you actually start thinking about it.

The magnet has to get close enough to pick up a chess piece through the board, but not stay so close that it drags the piece around when it is supposed to release it. The servo needs enough travel. The printed parts need to move smoothly. The mechanism also has to fit underneath the board with the rest of the XY system.

Simple enough on paper. Paper also does not have friction, tolerances, loose screws, or a servo that has decided today is not its day.

## Printing the first parts

<img width="555" height="319" alt="image" src="https://github.com/user-attachments/assets/8d20ef53-e2ae-495a-aad3-c7449f659c55" />

I 3D printed the parts for the linear-servo mechanism.

This was the first actual piece of the pickup system. Until now, “the magnet will pick up the pieces” was mostly a sentence with a lot of confidence behind it. Now there was a physical assembly meant to hold the magnet and move it vertically underneath the board.

It still needed to be attached to the servo, tested, adjusted, and probably redesigned at least once. But that is normal. A 3D print is not the final answer; it is the first time your assumptions get to meet plastic.

## What still needed to survive reality

Printing the parts was progress. It was not proof that the pickup system worked.

There were still a few fairly important questions:

* Is the servo strong enough to move the magnet reliably?
* How much vertical travel does the magnet actually need?
* Can the magnet pick up a chess piece through the board?
* Will it release the piece cleanly?
* Does the mechanism bind, flex, or get in the way of the XY carriage?

The plan was there. The printed parts were there. The physics was still waiting to give its opinion.
