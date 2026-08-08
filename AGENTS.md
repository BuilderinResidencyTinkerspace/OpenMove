# OpenMove agent guide

OpenMove is an experimental automatic chessboard project. Work incrementally and keep the repository as the source of truth.

## Read first

1. Read `.agents/PROJECT_MEMORY.md`.
2. Read the relevant documentation and files before making an assumption.
3. For OpenMove-specific work, read `.agents/skills/openmove-project/SKILL.md`.

## Current priority

Validate the XY motion prototype. Do not begin position sensing, chess logic, Stockfish, or online integration unless the user explicitly changes priorities.

## Engineering guardrails

- Treat the Arduino Uno, CNC Shield V3, two NEMA 17 motors, DRV8825 drivers, GT2 belts, 4 mm × 300 mm rods, servo, and permanent-magnet pickup as the current prototype direction.
- The 4 mm rods, XY kinematics, pickup mechanism, sensor choice, sensor-PCB architecture, host language, and serial protocol are not final decisions.
- Do not silently replace hardware, dimensions, power requirements, motor/driver choices, or sensor architecture. Explain the trade-off and record an approved significant decision.
- Never invent physical tests, measurements, test results, part availability, or completed work. Mark proposals as `Experimental` until validated.
- Keep Arduino responsibilities limited to low-level hardware control. Future chess logic and Stockfish run on a host computer.
- Prefer the smallest reversible experiment. Record both successful and failed results.

## Documentation and memory

- Preserve the Builder-in-Residence template: `docs/index.md` is the overview and `docs/week-01.md` through `docs/week-09.md` are weekly logs.
- Use the relevant weekly log for actual work, blockers, decisions, experiments, and links. Do not create separate meeting, experiment, decision, or BOM documentation sections.
- Update `.agents/PROJECT_MEMORY.md` when the factual project state changes. This is agent context, not public project documentation.
- Link implementation, CAD, or photos from the relevant weekly log when they exist.

## Change discipline

- Inspect the worktree first and preserve unrelated changes.
- Keep changes small and verify them proportionately.
- State uncertainty and ask before making a material decision that cannot be inferred safely.
