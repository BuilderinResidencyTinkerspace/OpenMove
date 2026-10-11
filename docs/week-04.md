# Week 4 — Putting Stockfish on a VM Because it sounds cool

<img width="1366" height="681" alt="image" src="https://github.com/user-attachments/assets/5e744349-0840-463c-98d1-a58d722c2f25" />

<img width="1366" height="681" alt="image" src="https://github.com/user-attachments/assets/03d60c08-0637-4418-8422-b5c6d6216cf3" />

**Goal this week:** Set up Stockfish on an Azure VM so OpenMove could eventually ask a chess engine for moves.

The mechanical side of OpenMove was slowly becoming real: motors, drivers, a servo, and a printed magnet mechanism.

The software side still had one very important missing piece: something had to decide what move the board should make.

That job belongs to Stockfish.

## Stockfish, but not directly from the board

The original idea was simple: the board detects a move, Stockfish thinks, and OpenMove physically replies.

In practice, the Arduino should not be running chess logic, validating full game state, talking to a server, and controlling motors at the same time. It already had enough going on.

So I set up Stockfish on an Azure VM instead.




The VM runs Stockfish inside a container and exposes a small API that accepts a chess position in FEN format, runs a limited search, and returns a move in UCI notation.

For example, the engine can receive a board position and return a move like `e2e4`.

That is a much cleaner boundary than letting an Arduino somehow become a chess computer, web client, and motion controller at the same time. It is also a good way to make debugging slightly less cursed later.

## Keeping the engine in its lane

The API was deliberately small.

It validates the position, runs Stockfish with a bounded search time, and sends back the engine move. The Azure VM service is not directly connected to the board, and it is not publicly exposed as a “free chess engine endpoint for the internet.”

The board still needs a host controller in between:

```text
Board sensors
     ↓
Host controller checks the game state
     ↓
Stockfish API returns a move
     ↓
Host controller validates it again
     ↓
Arduino moves the mechanism
```

That extra validation matters. A chess engine move is only useful if it still matches the physical board. If someone moves a piece while the engine is thinking, blindly sending the old response to the motors would be a very efficient way to make the board play a move from an alternate universe.

## What worked

* Stockfish was set up on an Azure VM.
* The service could receive a FEN position and return a UCI move.
* The engine ran inside a container.
* The service was kept separate from the Arduino and physical hardware.
* Basic API behaviour was tested before treating it as part of the project.

## What actually crosses the API boundary

FEN describes a chess position, including whose turn it is and the extra state
needed to interpret it. UCI move notation describes a move using its source and
destination squares. For example, `e2e4` asks for the piece on e2 to move to e4.
It says nothing about belt tension, servo travel, or whether a pawn is currently
stuck to the underside of the board.

The [API implementation](../stockfiash/app/main.py) accepts a complete FEN and
a search time from 50 to 5000 milliseconds. It rejects malformed or invalid
positions and games that are already over. A missing server API key prevents
move requests from being served, and a wrong client key is rejected.

The engine response can contain a best move and an optional ponder move. The
host still needs to decide whether the best move is executable on the physical
board. A legal capture is an excellent example: Stockfish can recommend it
long before this prototype has a validated place to put the captured piece.

## The deployment was intentionally small

The recorded deployment runs under `/home/mishal/openmove-stockfiash` on the
`az-vm` target. Docker binds the service to `127.0.0.1:8080`, with automatic
restart, a read-only filesystem, and limits of one CPU and 512 MiB of memory.
Access is local to the VM or through an SSH tunnel. Public DNS and TLS ingress
remain future work.

The recorded warm, VM-local response times were approximately 59 ms for a
50 ms search, 255 ms for 250 ms, and 506 ms for 500 ms. Those measurements do
not include a board talking over the internet. Concurrent searches queue inside
one instance, so they also do not establish multi-user throughput.

Five API unit tests were recorded as passing. The successful-move test mocks
the engine response; that checks the API contract, while the VM timing results
cover a different layer. Neither result says anything about a physical pawn.

## What I would carry forward

The useful outcome is a small engine service with a clear boundary. Keeping
that boundary also leaves room for a local Stockfish adapter later, when
internet access is unavailable or cloud hosting stops being convenient.

The [service README](../stockfiash/README.md) contains the run and deployment
instructions. With this experiment recorded, the immediate project priority
returns to the XY prototype. The board already has access to plenty of chess
intelligence. What it needs next is evidence that it can reach a square.
