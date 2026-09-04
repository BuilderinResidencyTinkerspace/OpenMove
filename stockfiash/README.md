# OpenMove cloud Stockfish service

Experimental host-side service for asking Stockfish for a move. The directory is
named `stockfiash` because that was the requested name; renaming it to `stockfish`
later would reduce typo tax.

## Current experimental deployment

The service is deployed on the `az-vm` SSH target at
`/home/mishal/openmove-stockfiash`. Docker Compose binds it to
`127.0.0.1:8080`, so it is reachable only from that VM or through an SSH tunnel.
It is deliberately not published over plaintext internet traffic. Public access
still requires a DNS name plus TLS/authenticated ingress.

## Boundary

```text
physical board -> host chess controller -> HTTPS -> this API -> Stockfish
                         |                    |
                  owns game state       returns UCI move
```

The Arduino must not call this API. It should only receive physical motion
commands from the host. The host must validate that the returned UCI move still
matches its current game state before commanding motion.

## Local run

```bash
cd stockfiash
cp .env.example .env
# Replace the placeholder in .env with: openssl rand -hex 32
docker compose up --build
```

Smoke test:

```bash
curl http://localhost:8080/health
curl -X POST http://localhost:8080/v1/best-move \
  -H 'Content-Type: application/json' \
  -H "X-API-Key: $OPENMOVE_API_KEY" \
  -d '{"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","time_ms":500}'
```

The response contains UCI coordinates, for example:

```json
{"best_move":"e2e4","ponder":"e7e5"}
```

## Cloud deployment contract

Deploy the image to a managed container platform or a small VM with:

- HTTPS terminated by the platform/load balancer;
- `OPENMOVE_API_KEY` injected from a secret manager, never baked into the image;
- one request of container concurrency because one Stockfish process is shared;
- 1 CPU and 512 MiB memory as a starting point;
- request timeout above the API's maximum 5-second search;
- platform rate limiting and a small maximum instance count to cap cost;
- logs that exclude request headers and therefore the API key.

Build and push:

```bash
docker build -t REGION-docker.pkg.dev/PROJECT/REPOSITORY/openmove-stockfish:0.1.0 .
docker push REGION-docker.pkg.dev/PROJECT/REPOSITORY/openmove-stockfish:0.1.0
```

Compose binds to `127.0.0.1:8080` by default. Configure a TLS reverse proxy or a
managed platform to send traffic to container port `8080`. Do not expose raw UCI or
accept arbitrary engine options: that would hand strangers a convenient compute
abuse endpoint.

An API key is acceptable for a private prototype, but it is not serious
production authentication. Before public deployment, put this behind a gateway
with device identity (mTLS or short-lived OAuth tokens), quotas, and revocation.

## API rules

`POST /v1/best-move` accepts a complete FEN plus `time_ms` from 50 to 5000.
Malformed/illegal positions and completed games are rejected. Search time is
intentionally capped; callers cannot choose threads, hash, depth, or arbitrary
UCI commands.

## Failure behavior

Cloud is optional orchestration, not a safety system. If the call times out,
returns a move illegal in the host's current state, or the board state changed
while waiting, the host must stop and ask for resynchronization. Never move a
physical piece based only on an old network response. A later local Stockfish
adapter should implement the same `fen -> UCI move` contract for offline play.
