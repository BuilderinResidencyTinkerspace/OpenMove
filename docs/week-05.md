# Week 5 — Giving the Board a Few Commands

**Goal this week:** Control the prototype from a terminal and give it a way to
report what it was doing.

The motors had moved in the bench test. Now I needed something more useful
than uploading a sketch and watching the same movement repeat forever.

I wanted to ask for a small jog, check the position, move to a square, and stop
when something went wrong. So the Arduino got a serial controller, and I added
a [terminal UI](../code/openmove_tui.py) to send commands and read the replies.

## First, tell it where it is

The awkward part is that the Arduino does not know where the carriage is when
it starts. I have to put it at the origin myself and send `HOME`. That assigns
zero to the current position. There is no automatic search for a corner or a
limit switch yet.

The pieces need their own confirmation. `RESETBOARD` resets the internal board
to the standard chess position, so the real pieces have to be arranged that
way first. It would be convenient if the command also tidied the board for me,
but unfortunately I am still that part of the system.

Home and board confirmation are separate because I can know where the carriage
is while having the pieces in the wrong places, or the other way around.
After an upload, both need to be established again before chess movement.

## Sending a command is only half the conversation

The controller gained commands for status, jogging, square movement, the
magnet actuator, and an emergency stop. The current serial code uses `busy:`,
`ok:`, and `error:` replies, with `status:` and `info:` fields to report state.
That lets the interface wait for an answer instead of assuming a command worked
because it was sent.

A reported position still comes from counted motor steps. There is no sensor
checking that the carriage reached it. If the belt slips, the display can be
wrong even when the command finishes normally.

The [firmware](../code/openmove_chess/openmove_chess.ino) also keeps a small
chess position and rejects invalid moves. That is part of the current
prototype; the longer-term plan is to keep full chess logic on the host and
leave the Uno handling motion and the actuator. The code is not fully split
that way yet.

## Where I left it

I had a way to talk to the prototype and inspect its state. I had not tested a
physical chess move, measured square accuracy, or established clearance around
pieces. Those were still experimental.

The next job was small manual jogs: settle the origin, check both directions,
and measure the travel. That sounds fairly straightforward. The next few
entries explain how much room there was to get it wrong.

The current [desktop GUI](../code/openmove_gui.py) uses the same controller
alongside the terminal tool.
