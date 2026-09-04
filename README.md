# ♟️ OpenMove

### A chessboard that moves its own pieces.

OpenMove is an open-source experiment to build a physical chessboard that can
**detect moves, think with Stockfish, and physically move the pieces.**

Because apparently pressing a button to move a chess piece wasn't dramatic enough.

---

## What are we building?

The idea is pretty simple:

**You play a move → OpenMove understands it → the chess engine thinks → the board moves the piece.**

Under the board, an XY motion system does the dirty work, while magnetic sensing
helps the board understand where the pieces are.

Eventually, we want OpenMove to be able to play a complete game against you,
without anyone touching the pieces.

## Current status

The project is still validating the XY motion prototype. A separate,
experimental Stockfish API has also been built and deployed for early host-side
integration testing. It is not connected to the physical board yet.

## Repository layout

- `cad/` — mechanical drawings and reference assets
- `code/` — firmware and future host-controller software
- `docs/` — project overview and weekly Builder-in-Residence logs
- [`stockfiash/`](stockfiash/README.md) — experimental containerized Stockfish API

The `stockfiash` spelling is retained for compatibility with the initial
prototype directory. New chess logic belongs on the host computer; the Arduino
remains limited to low-level hardware control.
