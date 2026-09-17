#!/usr/bin/env python3
"""Interactive serial test console for the OpenMove chess controller."""

from __future__ import annotations

import argparse
import curses
import errno
import fcntl
import re
import os
import select
import termios
import time
from collections import deque


BAUD_RATES = {115200: termios.B115200}


class SerialPort:
    def __init__(self, path: str, baud: int = 115200) -> None:
        self.path = path
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            fcntl.ioctl(self.fd, termios.TIOCEXCL)
        except OSError:
            os.close(self.fd)
            raise
        attributes = termios.tcgetattr(self.fd)
        attributes[0] = 0
        attributes[1] = 0
        attributes[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
        attributes[3] = 0
        attributes[4] = BAUD_RATES[baud]
        attributes[5] = BAUD_RATES[baud]
        attributes[6][termios.VMIN] = 0
        attributes[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attributes)
        termios.tcflush(self.fd, termios.TCIFLUSH)

    def write_line(self, command: str) -> None:
        self.write_all(command.encode("ascii") + b"\n")

    def write_all(self, payload: bytes) -> None:
        deadline = time.monotonic() + 1
        while payload:
            if time.monotonic() > deadline:
                raise OSError("Serial write timed out")
            if not select.select([], [self.fd], [], 0.05)[1]:
                continue
            try:
                count = os.write(self.fd, payload)
            except BlockingIOError:
                continue
            payload = payload[count:]

    def emergency_stop(self) -> None:
        self.write_all(b"!\n")

    def read(self) -> bytes:
        chunks: list[bytes] = []
        while True:
            readable, _, _ = select.select([self.fd], [], [], 0)
            if not readable:
                break
            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                break
            if not chunk:
                raise OSError("Serial device disconnected")
            chunks.append(chunk)
        return b"".join(chunks)

    def close(self) -> None:
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


class OpenMoveTUI:
    def __init__(
        self,
        screen: curses.window,
        serial: SerialPort,
        home_on_start: bool = False,
        reset_on_start: bool = False,
    ) -> None:
        self.screen = screen
        self.serial = serial
        self.home_on_start = home_on_start
        self.reset_on_start = reset_on_start
        self.lines: deque[str] = deque(maxlen=200)
        self.startup_commands: deque[str] = deque()
        self.partial = ""
        self.last_command = "none"
        self.notice = "External power state is your responsibility."
        self.command_pending = False
        self.pending_since = 0.0
        self.running = True
        self.jog_mm = 10
        self.controller_ready = False

    def log(self, prefix: str, message: str) -> None:
        self.lines.append(f"{prefix} {message}")

    def send(self, command: str) -> None:
        if self.command_pending:
            return
        self.serial.write_line(command)
        self.last_command = command
        self.command_pending = True
        self.pending_since = time.monotonic()
        self.log(">", command)

    def poll_serial(self) -> None:
        data = self.serial.read()
        if not data:
            return
        text = data.decode("utf-8", errors="replace").replace("\r", "")
        combined = self.partial + text
        parts = combined.split("\n")
        self.partial = parts.pop()[-1024:]
        for line in parts:
            if line:
                self.log("<", line)
                if line.startswith("ready:"):
                    self.controller_ready = True
                if line.startswith(("ok:", "error:")):
                    self.command_pending = False
                    curses.flushinp() # Discard accumulated key repeat after a movement.
                    if line.startswith("error:"):
                        self.startup_commands.clear()
                    elif self.startup_commands:
                        self.send(self.startup_commands.popleft())

    def add(self, row: int, col: int, text: str, style: int = 0) -> None:
        height, width = self.screen.getmaxyx()
        if row < 0 or row >= height or col >= width:
            return
        try:
            self.screen.addnstr(row, col, text, max(0, width - col - 1), style)
        except curses.error:
            pass

    def draw(self) -> None:
        self.screen.erase()
        height, width = self.screen.getmaxyx()
        title_style = curses.A_BOLD
        danger_style = curses.A_BOLD | curses.A_REVERSE

        self.add(0, 0, "OpenMove Hardware Test TUI", title_style)
        self.add(1, 0, f"Port: {self.serial.path}   Last command: {self.last_command}")
        self.add(2, 0, "SPACE = EMERGENCY STOP (drivers off, magnet released, position lost)", danger_style)

        controls = [
            f"Motion ({self.jog_mm} mm):   arrows = X/Y jog   c = toggle 1/10 mm",
            "Origin:          h = set a1 square centre as HOME",
            "Actuator:        0 = retract/release   9 = extend/engage",
            "Position:        g = move magnet to a square centre, e.g. e3",
            "Chess:           m = enter move, e.g. e2e4",
            "Controller:      s = status   r = reset internal board   d = disable motors",
            "Application:     q = quit (does not change hardware state)",
        ]
        for index, line in enumerate(controls, start=4):
            self.add(index, 0, line)

        divider_row = 11
        self.add(divider_row, 0, "─" * max(1, width - 1))
        self.add(divider_row + 1, 0, f"Notice: {self.notice}", curses.A_BOLD)
        self.add(divider_row + 2, 0, "Serial log:", title_style)

        log_start = divider_row + 3
        visible_count = max(0, height - log_start - 1)
        for row_offset, line in enumerate(list(self.lines)[-visible_count:] if visible_count else []):
            self.add(log_start + row_offset, 0, line)

        self.screen.refresh()

    def prompt(self, label: str, maximum: int = 16) -> str | None:
        height, _ = self.screen.getmaxyx()
        entered = ""
        try:
            while True:
                self.poll_serial()
                self.draw()
                height, _ = self.screen.getmaxyx()
                self.add(height - 2, 0, label)
                self.add(height - 1, 0, "> " + entered)
                self.screen.refresh()
                key = self.screen.getch()
                if key in (ord(' '), ord('!')):
                    self.handle_key(ord(' '))
                    return None
                if key == 27:
                    return None
                if key in (10, 13):
                    return entered.strip()
                if key in (curses.KEY_BACKSPACE, 127, 8):
                    entered = entered[:-1]
                elif 33 <= key <= 126 and len(entered) < maximum:
                    entered += chr(key)
        except (curses.error, KeyboardInterrupt):
            return None
        finally:
            curses.noecho()
            curses.curs_set(0)
            self.screen.timeout(50)

    def confirm_home(self) -> None:
        answer = self.prompt("Carriage physically at a1 centre? Type YES: ", 8)
        if answer and answer.upper() == "YES":
            self.send("HOME")
            self.notice = "Manual origin sent. Confirm the controller replies ok."
        else:
            self.notice = "HOME cancelled. Type yes to confirm."

    def enter_move(self) -> None:
        move = self.prompt("Coordinate move (example e2e4): ", 8)
        if not move:
            self.notice = "Chess move cancelled."
            return
        move = move.lower()
        if not re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", move):
            self.notice = "Rejected locally: use e2e4 or e7e8q format."
            return
        self.send(move)
        self.notice = "Wait for ok/error before sending another command."

    def goto_square(self) -> None:
        square = self.prompt("Move to square centre (example e3): ", 3)
        if not square:
            self.notice = "Go-to cancelled."
            return
        square = square.lower()
        if not re.fullmatch(r"[a-h][1-8]", square):
            self.notice = "Rejected locally: use a square such as e3."
            return
        self.send("goto " + square)
        self.notice = "Magnet releases first. Wait for the controller reply."

    def confirm_board_reset(self) -> None:
        answer = self.prompt("Reset firmware occupancy to starting position? Type YES: ", 8)
        if answer and answer.upper() == "YES":
            self.send("RESETBOARD")
        else:
            self.notice = "Board-state reset cancelled."

    def handle_key(self, key: int) -> None:
        if key == ord(" "):
            self.serial.emergency_stop()
            self.command_pending = True
            self.pending_since = time.monotonic()
            self.last_command = "EMERGENCY STOP"
            self.log(">", "! (emergency stop)")
            self.notice = "EMERGENCY STOP SENT. Re-home before any movement."
            return
        if key in (ord("q"), ord("Q")):
            self.running = False
            return
        if self.command_pending:
            self.notice = "Wait for the controller reply; SPACE still triggers emergency stop."
            return

        if key == curses.KEY_RIGHT:
            self.send(f"jogx+{self.jog_mm}")
        elif key == curses.KEY_LEFT:
            self.send(f"jogx-{self.jog_mm}")
        elif key == curses.KEY_UP:
            self.send(f"jogy+{self.jog_mm}")
        elif key == curses.KEY_DOWN:
            self.send(f"jogy-{self.jog_mm}")
        elif key in (ord("c"), ord("C")):
            self.jog_mm = 10 if self.jog_mm == 1 else 1
            self.notice = f"Jog distance set to {self.jog_mm} mm."
        elif key in (ord("h"), ord("H")):
            self.confirm_home()
        elif key == ord("0"):
            self.send("actuator_retract")
        elif key == ord("9"):
            self.send("actuator_extend")
        elif key in (ord("m"), ord("M")):
            self.enter_move()
        elif key in (ord("g"), ord("G")):
            self.goto_square()
        elif key in (ord("s"), ord("S")):
            self.send("status")
        elif key in (ord("r"), ord("R")):
            self.confirm_board_reset()
        elif key in (ord("d"), ord("D")):
            self.send("DISABLE")
            self.notice = "Motors disabled; physical position is no longer trusted."

    def run(self) -> None:
        curses.curs_set(0)
        curses.noecho()
        curses.cbreak()
        self.screen.keypad(True)
        self.screen.timeout(50)
        self.log("*", "Connected. Waiting for Arduino startup/reset.")
        self.draw()
        startup_deadline = time.monotonic() + 6.0
        while time.monotonic() < startup_deadline and not self.controller_ready:
            self.poll_serial()
            self.draw()
            time.sleep(0.05)

        if self.home_on_start:
            self.startup_commands.append("HOME")
        if self.reset_on_start:
            self.startup_commands.append("RESETBOARD")
        self.startup_commands.append("status")
        self.log("*", "Running controller startup sequence.")
        self.send(self.startup_commands.popleft())

        while self.running:
            self.poll_serial()
            if self.command_pending and time.monotonic() - self.pending_since > 5.0:
                self.notice = "Awaiting completion. Controls stay locked; SPACE sends stop."
            self.draw()
            key = self.screen.getch()
            if key != -1:
                self.handle_key(key)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="OpenMove Arduino hardware-test TUI")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Arduino serial port")
    parser.add_argument("--baud", type=int, default=115200, choices=sorted(BAUD_RATES))
    parser.add_argument(
        "--home",
        action="store_true",
        help="set the current physical carriage position as a1 home at startup",
    )
    parser.add_argument(
        "--reset",
        "--reset-board",
        dest="reset_on_start",
        action="store_true",
        help="reset firmware occupancy to the standard board at startup",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        serial = SerialPort(args.port, args.baud)
    except OSError as error:
        if error.errno in (errno.EACCES, errno.EBUSY):
            print(f"Cannot open {args.port}: {error.strerror}. Close other serial monitors.")
        else:
            print(f"Cannot open {args.port}: {error}")
        return 1

    try:
        curses.wrapper(
            lambda screen: OpenMoveTUI(
                screen,
                serial,
                home_on_start=args.home,
                reset_on_start=args.reset_on_start,
            ).run()
        )
    except KeyboardInterrupt:
        serial.emergency_stop()
    except OSError as error:
        print(f"Serial connection failed: {error}. Physical position is unverified.")
        return 1
    finally:
        serial.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
