# Week 1

**Goal this week:**

## What we did

- Added an experimental, containerized Stockfish HTTP API scaffold under
  [`stockfiash/`](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/stockfiash).
  It validates a FEN position, runs a
  bounded Stockfish search, and returns a UCI move.
- Deployed it on the `az-vm` SSH target at
  `/home/mishal/openmove-stockfiash`, bound to VM loopback on port 8080. Verified
  health (200), missing authentication (401), malformed FEN (422), and a real
  Stockfish 17.1 search (200, `e2e4` with `e7e5` ponder from the initial board).
  It remains disconnected from the physical board.
- Benchmarked five warm VM-local requests at each search budget. Mean response
  times were about 59 ms (50 ms search), 106 ms (100 ms), 255 ms (250 ms),
  506 ms (500 ms), and 1006 ms (1000 ms). Three simultaneous 500 ms requests
  completed at roughly 0.51 s, 1.01 s, and 1.52 s because searches are serialized.

## Problems and blockers

- There is no host chess controller or validated board-state input yet, so an
  engine response cannot safely trigger physical movement.
- Public DNS/TLS ingress, device authentication beyond the prototype API key,
  rate limiting, and offline behavior are not selected or tested.

## Decisions

- Experimental: keep Stockfish behind a narrow `FEN -> UCI move` host-side API.
  The Arduino remains responsible only for low-level hardware control.
- Experimental: use one Stockfish process and one concurrent search per service
  instance for predictable resource use.

## Next week

- Continue validating XY motion. Before connecting the engine to motion, define
  and test a host-side adapter that rejects stale or illegal engine responses.
- Add public DNS/TLS ingress only when a real remote client needs it; do not
  expose the current HTTP port directly.

## Links

- Code: [`stockfiash/`](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/stockfiash)
- Photos / CAD:
