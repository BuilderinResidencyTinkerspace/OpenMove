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

The Uno firmware is currently experimental version 2.7 / protocol 7. It has a
manual-home workflow, guarded jog/square commands, actuator endpoints, and an
experimental non-capture PGN replay mode. The desktop GUI refuses motion if the
firmware reports an incompatible coordinate contract or lost board trust.

The physical coordinate contract is `a1 = (0, 0)`, positive X from a-to-h,
positive Y from rank 1-to-8, with a measured 44 mm square pitch. Small positive
X/Y jog directions have been physically confirmed; full-square accuracy and
repeatability have not.

An experimental [cloud Stockfish service](https://github.com/BuilderinResidencyTinkerspace/OpenMove/tree/main/stockfiash)
also provides a narrow authenticated `FEN → UCI move` API. It is not connected
to the board. A host controller must verify current game state before any engine
output can become hardware motion.

## How to read this journal

The weekly entries follow the build from the original idea through electronics,
pickup parts, serial control, coordinate corrections, and experimental replay.
Earlier entries describe the version used at that point. In particular,
Week 7's white-only firmware and Week 8's intermediate origin labels are
historical steps, not instructions to configure the current controller.

The original opening story, photos, and detailed September notes remain in
place. Week 0 still contains the original technical updates; Weeks 8 and 9
explain their context alongside later work.

**Record discrepancy:** Week 9 and the root README report that firmware 2.7
was AVR-compiled and uploaded, while agent memory explicitly says it was not.
The source declares 2.7 / protocol 7, but the installed version remains
unconfirmed in this documentation until those records are reconciled. See
[Week 9's evidence note](week-09.md#the-upload-record-needs-reconciliation).

## What is implemented, and what is measured

| Layer | Existing evidence | Still to establish |
| --- | --- | --- |
| Bench electronics | Coordinated slow motor/servo test reported working | Current limits, temperature difference, representative load |
| Board mapping | Measured 44 mm pitch; positive X/Y jog directions confirmed | Full-square error, repeatability, full-board reach |
| Pickup | Printed mechanism documented | Repeatable pickup, transport, release, neighbour clearance |
| Manual control | Firmware, terminal UI, desktop GUI | Physical validation of the complete operating workflow |
| PGN replay | Both-side software sequence and fake-controller checks | Physical replay, restoration, and recovery |
| Engine | Separate API tests and recorded VM measurements | Connection to a trusted physical board state |
| Position sensing | Proposed Hall grid | Sensor choice, PCB, scanning, and validation |

The documentation describes an intended host/Arduino split. The experimental
firmware currently contains chess-state and move-validation code as well as
motion control, so that architectural separation is still work to finish.

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

- [Week 00](week-00.md) — the original idea
- [Week 01](week-01.md) — power, drivers, and a measured VREF starting point
- [Week 02](week-02.md) — the first coordinated smoke test
- [Week 03](week-03.md) — the servo-driven magnet mechanism
- [Week 04](week-04.md) — an experimental Stockfish service
- [Week 05](week-05.md) — guarded serial control
- [Week 06](week-06.md) — making coordinates testable
- [Week 07](week-07.md) — white-side motion firmware
- [Week 08](week-08.md) — correcting the physical mapping
- [Week 09](week-09.md) — experimental PGN replay and trust recovery
