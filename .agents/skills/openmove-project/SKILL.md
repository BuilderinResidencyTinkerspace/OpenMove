---
name: openmove-project
description: Guide safe work on the OpenMove automatic chessboard project. Use when planning, changing, reviewing, testing, or writing its weekly project logs for firmware, host software, CAD, electronics, mechanics, and sensor work.
---

# OpenMove Project

## Start with evidence

Read `AGENTS.md` and `.agents/PROJECT_MEMORY.md`. Then inspect the relevant source and documentation. Treat them as more reliable than chat history.

Do not claim a physical action, measurement, or result unless it is recorded or supplied by the user. Say what is unknown.

## Work in the current phase

Prioritize the XY motion prototype. Keep later systems out of scope unless explicitly requested:

1. XY motion
2. Homing and calibration
3. Servo + permanent-magnet pickup
4. Piece movement
5. Position detection
6. Chess state, engine, and online integrations

## Preserve decisions and uncertainty

- Preserve the current prototype hardware unless the user approves a change: Arduino Uno, CNC Shield V3, NEMA 17 motors, DRV8825 drivers, GT2 belts, 4 mm × 300 mm rods, servo, and permanent magnet.
- Mark the rod choice, kinematics, pickup mechanism, Hall sensor, sensor PCB, host language, and serial protocol as experimental or undecided unless new evidence changes that status.
- Before proposing a material hardware change, explain why, what it affects, alternatives, and whether it is experimental or final.
- Keep the Arduino focused on low-level control; plan complex chess logic and Stockfish for a host computer.

## Document truthfully

- Keep the BIR overview and weekly-log structure intact.
- Use the relevant `docs/week-*.md` for all project reporting: actual work, blockers, decisions, experiments, next steps, and links.
- Update `.agents/PROJECT_MEMORY.md` only to preserve factual working context for agents; it is not a documentation section.

## Finish safely

Summarize changed files, validation performed, remaining uncertainty, and the next physical or engineering action. Do not create implementation or documentation that implies unperformed work.
