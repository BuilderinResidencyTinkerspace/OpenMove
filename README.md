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

