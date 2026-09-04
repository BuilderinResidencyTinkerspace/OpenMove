# OpenMove

OpenMove is an experimental automatic chessboard: a player makes a move, a host
computer maintains the game state and asks Stockfish for a reply, and an XY
mechanism moves the responding piece under the board.

The project is not a finished autonomous chessboard. The current engineering
priority is validating repeatable XY motion. Position sensing, the pickup
mechanism, full chess-state integration, and end-to-end play remain unvalidated.

## System boundary

```text
player move
    ↓
position sensing (planned)
    ↓
host controller ──HTTPS──> Stockfish API
    ↓                         ↓
validates game state      returns UCI move
    ↓
Arduino motion command
    ↓
XY mechanism + magnetic pickup
```

The Arduino is intentionally limited to low-level hardware control. Chess rules,
game state, Stockfish, UI, and future online integrations belong on the host
computer. An engine response must be validated against the host's current game
state before any physical movement begins.

## Current prototype

| Area | Direction | Status |
| --- | --- | --- |
| Controller | Arduino Uno + CNC Shield V3 | Available; motion unvalidated |
| Motion | 2 × NEMA 17, 2 × DRV8825, GT2 belts | Experimental |
| Linear support | 2 × 4 mm × 300 mm stainless-steel rods | Experimental |
| Pickup | Servo-operated permanent magnet | Not finalized |
| Motion power | 12 V target | Unvalidated |
| Position sensing | Planned 8 × 8 Hall-effect sensor grid | Architecture undecided |
| Chess engine | Containerized Stockfish 17.1 API | Deployed experimentally |
| Host controller | Not implemented | Blocker for integration |

## Cloud Stockfish API

The experimental service lives in [`stockfiash/`](stockfiash/README.md). The
misspelled directory name is retained for compatibility with the initial
prototype.

It is deployed on the `az-vm` SSH target, restart-enabled and bound to
`127.0.0.1:8080`. It is deliberately not public: DNS, TLS ingress, stronger
device authentication, and rate limiting still need to be designed.

Verified warm VM-local response times are approximately 59 ms for a 50 ms
search, 255 ms for a 250 ms search, and 506 ms for a 500 ms search. Searches are
serialized within one service instance.

Run it locally:

```bash
cd stockfiash
cp .env.example .env
# Replace the placeholder API key in .env.
docker compose up --build
```

See the [service documentation](stockfiash/README.md) for API requests,
deployment constraints, security requirements, and failure handling.

## Repository layout

- `cad/` — mechanical drawings and reference assets
- `code/` — firmware and future host-controller software
- `docs/` — project overview and weekly Builder-in-Residence logs
- `stockfiash/` — experimental Stockfish HTTP API, tests, and container setup

## Documentation

- [Project overview](docs/index.md)
- [Week 1 engineering log](docs/week-01.md)
- [Weeks 2–9](docs/) — reserved weekly logs; no work is claimed until recorded

## Next engineering steps

1. Validate XY travel, repeatability, belt tension, rod rigidity, and failure
   behavior on the physical prototype.
2. Add and validate homing/calibration.
3. Validate the magnetic pickup before attempting complete piece movement.
4. Define a host-controller adapter that rejects stale or illegal engine moves.

Cloud chess is not the current critical path. If the mechanics cannot move
reliably, a brilliant engine merely calculates which piece the machine will
misplace next.
