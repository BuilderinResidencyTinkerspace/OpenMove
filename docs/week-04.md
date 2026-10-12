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

## The small API behind it

The [service](../stockfiash/README.md) takes a complete FEN position and a
search time between 50 and 5000 milliseconds. It checks the position and API
key before asking Stockfish for a move. The response contains a best move and,
sometimes, a ponder move.

FEN carries the board state; a move such as `e2e4` just names the source and
destination. The host still has to check whether the physical board can carry
it out. Stockfish has no idea that I have not figured out where captured pieces
are supposed to go.

The recorded deployment is under `/home/mishal/openmove-stockfiash` on `az-vm`,
listening on `127.0.0.1:8080`. It uses one CPU and 512 MiB of memory, a read-only
filesystem, and automatic restart. I can reach it locally or through an SSH
tunnel; public DNS and TLS access are still pending.

The recorded warm responses on the VM took about 59 ms for a 50 ms search,
255 ms for 250 ms, and 506 ms for 500 ms. Requests queue within one instance.
Those timings cover the service on the VM, not an internet round trip from the
board.

Five API tests were recorded as passing. The successful-response test uses a
mock engine, so that checks the request and response handling separately from
the actual Stockfish measurements.

A local engine could use the same interface later. For now, the service exists
and the next job is back at the board: getting the carriage to move reliably.
