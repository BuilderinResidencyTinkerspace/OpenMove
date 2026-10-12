# Week 6 — Figuring Out What “Go to e4” Means

**Goal this week:** Make the board coordinates clear enough to test.

`GOTO e4` is a very short command for something with this many assumptions.
Where is a1? Which way is positive X? How far apart are the squares? And what
do the two motors need to do to get there?

I needed those answers to agree between the firmware, the interface, and the
actual board. Up to this point, too much depended on how I thought the machine
was facing.

## From a square to two motors

The firmware first turns the square into an X/Y distance using the origin and
square spacing. It then turns that distance into motor steps using the
experimental H-bot mapping and calibration.

Both motors contribute to carriage movement in that mapping. Reversing one
motor is not necessarily the same as reversing the board's X axis. I needed
to keep those changes separate so I could tell which assumption a test was
actually checking.

At this stage, the origin label, directions, square pitch, and steps per
millimetre were still being worked out. The 4 mm rods, belt arrangement, and
pickup mechanism were also prototype choices.

## Making the firmware explain itself

The controller's status reporting was expanded around the manual-home
workflow. Later revisions report the origin and axis directions explicitly,
so the interface can check them before allowing chess movement.

That matters because two programs can agree that `e4` is valid text while
meaning completely different places. A connection opening successfully does
not catch that. The host needs to ask which mapping the firmware is using.

The [physical tests in Week 8](week-08.md) are where the wrong assumptions
became obvious. I have kept that history because the intermediate answers
explain why the final mapping changed.

## What still needed measuring

The Arduino counts the steps it sends. It cannot tell whether the carriage
missed one or whether I moved it by hand. Setting a travel limit in code also
does not measure how much space the real mechanism has.

My next test was to check direction with small jogs, then measure the distance,
then see whether repeated moves returned to the same point. Until then, a
successful compile and upload only meant I was ready to test the mapping.

This week was mostly about making the assumptions visible. I still needed to
put a ruler beside the machine.
