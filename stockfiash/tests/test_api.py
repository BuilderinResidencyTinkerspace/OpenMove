from unittest.mock import AsyncMock, patch

from fastapi.testclient import TestClient

from app.engine import MoveResult
from app.main import app


client = TestClient(app)
START_FEN = "rn1qkbnr/pppb1ppp/3pp3/8/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 1 4"


def test_health_needs_no_key() -> None:
    assert client.get("/health").json() == {"status": "ok"}


def test_move_requires_configured_key(monkeypatch) -> None:
    monkeypatch.delenv("OPENMOVE_API_KEY", raising=False)
    assert client.post("/v1/best-move", json={"fen": START_FEN}).status_code == 503


def test_move_rejects_bad_key(monkeypatch) -> None:
    monkeypatch.setenv("OPENMOVE_API_KEY", "correct-key")
    response = client.post(
        "/v1/best-move",
        headers={"X-API-Key": "wrong-key"},
        json={"fen": START_FEN},
    )
    assert response.status_code == 401


def test_move_rejects_invalid_fen(monkeypatch) -> None:
    monkeypatch.setenv("OPENMOVE_API_KEY", "correct-key")
    response = client.post(
        "/v1/best-move",
        headers={"X-API-Key": "correct-key"},
        json={"fen": "garbage"},
    )
    assert response.status_code == 422


def test_move_returns_uci_move(monkeypatch) -> None:
    monkeypatch.setenv("OPENMOVE_API_KEY", "correct-key")
    with patch(
        "app.main.engine.play",
        new=AsyncMock(return_value=MoveResult("g8f6", None)),
    ):
        response = client.post(
            "/v1/best-move",
            headers={"X-API-Key": "correct-key"},
            json={"fen": START_FEN, "time_ms": 250},
        )
    assert response.status_code == 200
    assert response.json() == {"best_move": "g8f6", "ponder": None}
