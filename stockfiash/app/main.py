import hmac
import os
from contextlib import asynccontextmanager

import chess
from fastapi import Depends, FastAPI, Header, HTTPException, status
from pydantic import BaseModel, Field

from app.engine import StockfishService


engine = StockfishService()


@asynccontextmanager
async def lifespan(_: FastAPI):
    yield
    await engine.close()


app = FastAPI(title="OpenMove Stockfish API", version="0.1.0", lifespan=lifespan)


class MoveRequest(BaseModel):
    fen: str = Field(description="A complete six-field Forsyth-Edwards Notation position")
    time_ms: int = Field(default=500, ge=50, le=5_000)


class MoveResponse(BaseModel):
    best_move: str
    ponder: str | None


def require_api_key(x_api_key: str | None = Header(default=None)) -> None:
    expected = os.getenv("OPENMOVE_API_KEY")
    if not expected:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="server API key is not configured",
        )
    if x_api_key is None or not hmac.compare_digest(x_api_key, expected):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="invalid API key",
        )


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.post(
    "/v1/best-move",
    response_model=MoveResponse,
    dependencies=[Depends(require_api_key)],
)
async def best_move(request: MoveRequest) -> MoveResponse:
    try:
        board = chess.Board(request.fen)
    except ValueError as exc:
        raise HTTPException(status_code=422, detail="invalid FEN") from exc

    if not board.is_valid():
        raise HTTPException(status_code=422, detail="illegal chess position")
    if board.is_game_over():
        raise HTTPException(status_code=409, detail="the game is already over")

    try:
        result = await engine.play(board, request.time_ms)
    except (ValueError, OSError, TimeoutError) as exc:
        raise HTTPException(status_code=503, detail="engine unavailable") from exc
    return MoveResponse(best_move=result.best_move, ponder=result.ponder)
