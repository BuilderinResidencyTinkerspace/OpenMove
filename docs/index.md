# OpenMove

- **Program:** Builder-in-Residence
- **Phase:** Early mechanical prototype
- **Immediate objective:** Validate an XY mechanism that can move a magnetic
  pickup repeatably between chessboard squares.

## Problem

A conventional chessboard cannot detect play, calculate a response, or move the
responding piece. OpenMove explores whether those functions can be combined in a
physical board without pushing complex software and game logic onto a small
microcontroller.

## Proposed system

```text
board sensors → host chess controller → Stockfish
                       ↓
               validated UCI move
                       ↓
            Arduino low-level control
                       ↓
        XY mechanism + magnetic pickup
```

This is a proposed end-to-end architecture, not a validated system. The Arduino
handles motion and hardware control. A host computer will own chess rules, game
state, Stockfish, UI, and future online integrations.

## Current hardware direction

| Component | Current direction | Confidence |
| --- | --- | --- |
| Motion controller | Arduino Uno with CNC Shield V3 | Prototype direction |
| Motors and drivers | 2 × NEMA 17 with 2 × DRV8825 | Not yet validated |
| Transmission | GT2 belt drive | Experimental |
| Linear support | 2 × 4 mm × 300 mm stainless-steel rods | Experimental |
| Pickup | Servo-operated permanent magnet | Not finalized |
| Power | 12 V motion-power target | Not validated |
| Position detection | Planned 8 × 8 Hall-effect grid | Sensor and PCB undecided |

The rod choice, XY kinematics, pickup mechanism, sensor architecture, host
language, and serial protocol are not final decisions.

## Current software status

An experimental [cloud Stockfish service](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/stockfiash)
now provides a
narrow authenticated `FEN → UCI move` API. It runs Stockfish 17.1 in a constrained
container on the `az-vm` SSH target.

The deployment is bound to VM loopback at `127.0.0.1:8080`, not the public
internet. Health, authentication rejection, invalid-position rejection, and a
real engine search have been tested. It is not connected to the chessboard.

The missing host controller is a deliberate boundary: cloud output must never
trigger hardware directly. The host must confirm that the board state has not
changed and that the returned move remains legal.

## Evidence so far

- Stockfish API unit tests: 5 passing.
- Stockfish API deployment: running with a 1 CPU and 512 MiB limit, read-only
  filesystem, and automatic restart.
- Warm VM-local response: approximately 59 ms for a 50 ms search, 255 ms for a
  250 ms search, and 506 ms for a 500 ms search.
- Concurrent searches queue inside one instance by design.
- No physical XY accuracy, repeatability, pickup, sensing, or complete-game test
  has been recorded yet.

## Risks and blockers

- The XY mechanism and 4 mm rods may not be rigid or repeatable enough.
- Homing and calibration have not been implemented or measured.
- The magnetic pickup/release method and capture handling are unresolved.
- Position sensing is only a plan; its sensor and PCB architecture are unknown.
- There is no host chess controller to reconcile physical and logical state.
- Public engine access still needs DNS, TLS, device identity, quotas, and
  revocation. The current API key is prototype-grade authentication.
- Cloud dependence introduces latency and loss of offline play, so a compatible
  local-engine adapter should remain possible.

## Work sequence

1. XY motion
2. Homing and calibration
3. Servo and magnetic pickup
4. Reliable piece movement and captures
5. Position detection
6. Host chess state and engine integration
7. Online integration

This order matters. Building higher layers before motion works would create a
clean software demo attached to unreliable hardware—which is a fancy way to
avoid testing the actual invention.

## Repository map

- [`docs/`](index.md) — this overview and weekly engineering logs
- [`code/`](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/code) — firmware and future host software
- [`cad/`](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/cad) — mechanical designs and references
- [`stockfiash/`](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/stockfiash) — experimental Stockfish API

## Weekly logs

- [Week 1](week-01.md) — cloud engine scaffold, deployment, tests, and benchmarks
- [Week 2](week-02.md)
- [Week 3](week-03.md)
- [Week 4](week-04.md)
- [Week 5](week-05.md)
- [Week 6](week-06.md)
- [Week 7](week-07.md)
- [Week 8](week-08.md)
- [Week 9](week-09.md)

Empty weekly logs are placeholders and do not imply completed work.
