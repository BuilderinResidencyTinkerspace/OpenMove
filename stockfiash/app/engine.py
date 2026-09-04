import asyncio
import os
from dataclasses import dataclass

import chess
import chess.engine


@dataclass(frozen=True)
class MoveResult:
    best_move: str
    ponder: str | None


class StockfishService:
    """Own one Stockfish process and allow one search at a time."""

    def __init__(self) -> None:
        self._path = os.getenv("STOCKFISH_PATH", "/usr/local/bin/stockfish")
        self._engine: chess.engine.SimpleEngine | None = None
        self._lock = asyncio.Lock()

    def _start(self) -> chess.engine.SimpleEngine:
        engine = chess.engine.SimpleEngine.popen_uci(self._path, timeout=5.0)
        engine.configure(
            {
                "Threads": int(os.getenv("STOCKFISH_THREADS", "1")),
                "Hash": int(os.getenv("STOCKFISH_HASH_MB", "64")),
            }
        )
        return engine

    def _play_blocking(self, board: chess.Board, time_ms: int) -> MoveResult:
        if self._engine is None:
            self._engine = self._start()
        try:
            result = self._engine.play(
                board,
                chess.engine.Limit(time=time_ms / 1000),
                game=object(),
            )
        except (chess.engine.EngineError, chess.engine.EngineTerminatedError):
            self._engine = self._start()
            result = self._engine.play(
                board,
                chess.engine.Limit(time=time_ms / 1000),
                game=object(),
            )
        if result.move is None:
            raise ValueError("the position has no legal move")
        return MoveResult(
            best_move=result.move.uci(),
            ponder=result.ponder.uci() if result.ponder else None,
        )

    async def play(self, board: chess.Board, time_ms: int) -> MoveResult:
        async with self._lock:
            return await asyncio.to_thread(self._play_blocking, board, time_ms)

    async def close(self) -> None:
        async with self._lock:
            if self._engine is not None:
                await asyncio.to_thread(self._engine.quit)
                self._engine = None
