# Week 7 — Letting the Board Handle White

**Date:** 18 September 2026

For firmware 0.9, I split the turns: the controller would move White, and a
human would move Black. After making a Black move on the board, I would enter
it so the firmware could update its internal position.

That kept the first version smaller. It also meant that entering a Black move
must not make the carriage perform it again. The piece was already there.
Later versions add both-side replay, but this was the arrangement for this
week's build.

## Keeping the real board and the stored board together

The firmware checked the entered move before updating its position. It could
accept a legal Black capture as a state update because the human had already
moved the pieces and removed the captured one.

If the Black entry was rejected, the internal board became untrusted. That
might seem strict for a typo, but the physical move had already happened. The
controller could not safely assume the pieces still matched its old position.

Promotion stayed blocked for both colours, and king capture was rejected.
Automated captures, castling, and en passant were also outside what the physical
prototype could support. I also guarded the step timing calculation against
unsigned underflow.

## The knight problem

A knight can jump in chess. My magnet cannot.

To move one through a crowded board, the piece needs enough room along the
whole route. The proposed route used lanes along square boundaries, but I had
not measured that clearance against the largest piece base. Automated knight
moves were disabled in this build until that could be checked.

The current source has a later experimental planner that temporarily moves
blocking pieces. That is a different approach, and it still needs physical
testing. It does not change what was tested here.

## Uploaded, with movement still to test

Firmware 0.9 compiled for the Uno using 12,880 bytes of flash and 527 bytes of
SRAM, then uploaded to `/dev/ttyUSB0`. No HOME or movement command was issued
during that upload.

So I had the new firmware installed, but no physical move result for it yet.
Accuracy, routing, and pickup clearance were still untested. Before getting
further into turn handling, I needed to settle the coordinate mapping—which
became [the next entry](week-08.md).
