#!/usr/bin/env python3
"""Native desktop controller for the OpenMove white-piece prototype."""

from __future__ import annotations

import argparse
import threading
import time
from collections import deque
from collections.abc import Callable

import chess
from PySide6.QtCore import QObject, QPoint, QRect, QSize, Qt, QTimer, Signal
from PySide6.QtGui import QAction, QColor, QFont, QKeySequence, QPainter, QPen
from PySide6.QtWidgets import (
    QApplication,
    QGridLayout,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSizePolicy,
    QSplitter,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from openmove_tui import SerialPort


PIECES = {
    "K": "♔", "Q": "♕", "R": "♖", "B": "♗", "N": "♘", "P": "♙",
    "k": "♚", "q": "♛", "r": "♜", "b": "♝", "n": "♞", "p": "♟",
}

EXPECTED_MAPPING = {
    "protocol": "6",
    "home_square": "a1",
    "x_axis": "a-to-h",
    "y_axis": "1-to-8",
    "white_ranks": "1-2",
    "x_motor_direction": "reverse",
}


def parse_white_move(board: chess.Board, text: str) -> chess.Move:
    board.turn = chess.WHITE
    notation = text.strip()
    try:
        move = chess.Move.from_uci(notation.lower())
        if move not in board.legal_moves:
            raise ValueError
    except ValueError:
        try:
            move = board.parse_san(notation)
        except ValueError as error:
            raise ValueError("Enter a legal white move such as e2e4 or Nf3") from error

    if board.color_at(move.from_square) != chess.WHITE:
        raise ValueError("Only white pieces are automated")
    if board.is_capture(move):
        raise ValueError("White captures are disabled until removal is validated")
    if board.is_castling(move):
        raise ValueError("Castling is not yet supported")
    if move.promotion:
        raise ValueError("Promotion is not yet supported")
    return move


def executable_moves(board: chess.Board) -> list[chess.Move]:
    board.turn = chess.WHITE
    return [
        move
        for move in board.legal_moves
        if not board.is_capture(move)
        and not board.is_castling(move)
        and not move.promotion
    ]


class SerialController(QObject):
    connection_changed = Signal(bool, str)
    line_received = Signal(str)
    status_changed = Signal(dict)
    command_finished = Signal(str, str)
    command_failed = Signal(str, str)

    def __init__(self) -> None:
        super().__init__()
        self.serial: SerialPort | None = None
        self.port = "/dev/ttyUSB0"
        self.status: dict[str, str] = {
            "homed": "0",
            "board_confirmed": "0",
            "side_to_move": "white",
        }
        self.pending_command: str | None = None
        self._response: str | None = None
        self._partial = ""
        self._condition = threading.Condition()
        self._command_lock = threading.Lock()
        self._stop_reader = threading.Event()
        self._reader: threading.Thread | None = None

    @property
    def connected(self) -> bool:
        return self.serial is not None

    def connect_port(self, port: str) -> None:
        if self.pending_command:
            raise RuntimeError("Abort the active command before reconnecting")
        self.disconnect(force=True)
        try:
            self.serial = SerialPort(port)
        except OSError as error:
            raise RuntimeError(f"Cannot open {port}: {error}") from error
        self.port = port
        self.status = {"homed": "0", "board_confirmed": "0", "side_to_move": "white"}
        self._partial = ""
        self._stop_reader.clear()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()
        self.connection_changed.emit(True, port)

    def disconnect(self, force: bool = False) -> None:
        if self.pending_command and not force:
            raise RuntimeError("Abort the active command before disconnecting")
        self._stop_reader.set()
        serial = self.serial
        self.serial = None
        if serial:
            serial.close()
        reader = self._reader
        if reader and reader is not threading.current_thread():
            reader.join(timeout=0.5)
        self._reader = None
        with self._condition:
            if self.pending_command:
                self._response = "error:serial-disconnected"
            self._condition.notify_all()
        if serial:
            self.connection_changed.emit(False, self.port)

    def _read_loop(self) -> None:
        while not self._stop_reader.is_set():
            serial = self.serial
            if serial is None:
                return
            try:
                data = serial.read()
            except OSError as error:
                try:
                    serial.close()
                except OSError:
                    pass
                self.serial = None
                with self._condition:
                    self._response = "error:serial-disconnected"
                    self._condition.notify_all()
                self.command_failed.emit(self.pending_command or "serial", str(error))
                self.connection_changed.emit(False, self.port)
                return
            if data:
                self._consume(data.decode("utf-8", errors="replace").replace("\r", ""))
            self._stop_reader.wait(0.02)

    def _consume(self, text: str) -> None:
        parts = (self._partial + text).split("\n")
        self._partial = parts.pop()[-1024:]
        for line in parts:
            if not line:
                continue
            self.line_received.emit(line)
            if line.startswith("status:"):
                self._update_status(line.removeprefix("status:"))
            elif line.startswith("info:") and "=" in line:
                self._update_status(line.removeprefix("info:"))
            if line.startswith(("ok:", "error:")):
                with self._condition:
                    self._response = line
                    self._condition.notify_all()

    def _update_status(self, fields: str) -> None:
        for field in fields.split(","):
            if "=" in field:
                key, value = field.split("=", 1)
                self.status[key] = value
        self.status_changed.emit(dict(self.status))

    def send_async(self, command: str, timeout: float = 90.0) -> None:
        with self._condition:
            if not self.connected:
                raise RuntimeError("Controller is not connected")
            if self.pending_command:
                raise RuntimeError(f"Wait for {self.pending_command} to finish")
            self.pending_command = command
        threading.Thread(target=self._command_worker, args=(command, timeout), daemon=True).start()

    def _command_worker(self, command: str, timeout: float) -> None:
        try:
            response = self._command(command, timeout)
        except RuntimeError as error:
            self.command_failed.emit(command, str(error))
        else:
            self.command_finished.emit(command, response)

    def _command(self, command: str, timeout: float) -> str:
        with self._command_lock:
            with self._condition:
                serial = self.serial
                if serial is None:
                    raise RuntimeError("Controller is not connected")
                self._response = None
                self.line_received.emit(f"> {command}")
                try:
                    serial.write_line(command)
                except OSError as error:
                    self.pending_command = None
                    raise RuntimeError(f"Serial write failed: {error}") from error
                deadline = time.monotonic() + timeout
                while self._response is None:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        self.pending_command = None
                        self.abort()
                        raise RuntimeError(f"{command} timed out; abort sent")
                    self._condition.wait(remaining)
                response = self._response
                self._response = None
                self.pending_command = None
        if response.startswith("error:"):
            raise RuntimeError(response.removeprefix("error:").replace("-", " "))
        return response

    def abort(self) -> None:
        serial = self.serial
        if serial is None:
            raise RuntimeError("Controller is not connected")
        try:
            serial.emergency_stop()
        except OSError as error:
            raise RuntimeError(f"Abort send failed: {error}") from error
        self.line_received.emit("> ! (motion abort)")


class ChessBoardWidget(QWidget):
    square_clicked = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self.board = chess.Board()
        self.selected: str | None = None
        self.targets: set[str] = set()
        self.setMinimumSize(360, 360)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def sizeHint(self) -> QSize:
        return QSize(680, 680)

    def board_rect(self) -> QRect:
        side = min(self.width(), self.height())
        return QRect((self.width() - side) // 2, (self.height() - side) // 2, side, side)

    def paintEvent(self, _event) -> None:
        del _event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = self.board_rect()
        cell = rect.width() / 8.0
        piece_font = QFont("DejaVu Sans", max(18, int(cell * 0.62)))
        coordinate_font = QFont("DejaVu Sans Mono", max(8, int(cell * 0.12)), QFont.Weight.Bold)

        for rank_index in range(8):
            rank = 7 - rank_index
            for file in range(8):
                square_name = chess.square_name(chess.square(file, rank))
                square_rect = QRect(
                    round(rect.left() + file * cell),
                    round(rect.top() + rank_index * cell),
                    round(cell + 0.5),
                    round(cell + 0.5),
                )
                painter.fillRect(square_rect, QColor("#e8ddc2") if (file + rank) % 2 else QColor("#6f8f69"))
                if square_name == self.selected:
                    painter.setPen(QPen(QColor("#e3b341"), max(3, int(cell * 0.07))))
                    painter.drawRect(square_rect.adjusted(2, 2, -2, -2))
                if square_name in self.targets:
                    painter.setPen(Qt.PenStyle.NoPen)
                    painter.setBrush(QColor("#1e6545"))
                    radius = max(5, int(cell * 0.12))
                    painter.drawEllipse(square_rect.center(), radius, radius)

                piece = self.board.piece_at(chess.square(file, rank))
                if piece:
                    painter.setFont(piece_font)
                    painter.setPen(QColor("#17211b"))
                    painter.drawText(square_rect, Qt.AlignmentFlag.AlignCenter, PIECES[piece.symbol()])

                painter.setFont(coordinate_font)
                painter.setPen(QColor("#24342a"))
                if rank == 0:
                    painter.drawText(square_rect.adjusted(0, 0, -3, -2), Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignBottom, chr(97 + file))
                if file == 0:
                    painter.drawText(square_rect.adjusted(3, 2, 0, 0), Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop, str(rank + 1))

        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.setPen(QPen(QColor("#293a30"), 8))
        painter.drawRect(rect.adjusted(4, 4, -4, -4))

    def mousePressEvent(self, event) -> None:
        if event.button() != Qt.MouseButton.LeftButton:
            return
        rect = self.board_rect()
        point: QPoint = event.position().toPoint()
        if not rect.contains(point):
            return
        cell = rect.width() / 8.0
        file = min(7, int((point.x() - rect.left()) / cell))
        rank = 7 - min(7, int((point.y() - rect.top()) / cell))
        self.square_clicked.emit(chess.square_name(chess.square(file, rank)))


class OpenMoveWindow(QMainWindow):
    def __init__(self, default_port: str) -> None:
        super().__init__()
        self.setWindowTitle("OpenMove Controller")
        self.board = chess.Board()
        self.serial = SerialController()
        self.pending_success: Callable[[], None] | None = None
        self.logs: deque[str] = deque(maxlen=2000)
        self.jog_mm = 10
        self._build_ui(default_port)
        self._connect_signals()
        self._render_board()
        self._set_connected(False)
        self.board_widget.setFocus()

    def _build_ui(self, default_port: str) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QHBoxLayout(central)
        root.setContentsMargins(12, 12, 12, 12)
        splitter = QSplitter(Qt.Orientation.Horizontal)
        root.addWidget(splitter)
        self.board_widget = ChessBoardWidget()
        splitter.addWidget(self.board_widget)

        panel = QWidget()
        panel.setMinimumWidth(320)
        panel.setMaximumWidth(420)
        panel_layout = QVBoxLayout(panel)
        panel_layout.setSpacing(8)
        title = QLabel("OpenMove")
        title.setObjectName("title")
        panel_layout.addWidget(title)
        self.connection_label = QLabel("Disconnected")
        self.connection_label.setObjectName("connection")
        panel_layout.addWidget(self.connection_label)

        connection_row = QHBoxLayout()
        self.port_input = QLineEdit(default_port)
        self.connect_button = QPushButton("Connect")
        connection_row.addWidget(self.port_input, 1)
        connection_row.addWidget(self.connect_button)
        panel_layout.addLayout(connection_row)

        status_grid = QGridLayout()
        self.home_status = self._status_item(status_grid, 0, "Origin")
        self.board_status = self._status_item(status_grid, 1, "Board")
        self.command_status = self._status_item(status_grid, 2, "Command")
        self.command_status.setText("Idle")
        panel_layout.addLayout(status_grid)

        move_row = QHBoxLayout()
        self.move_input = QLineEdit()
        self.move_input.setPlaceholderText("e2e4 or Nf3")
        self.move_button = QPushButton("Move White")
        self.move_button.setObjectName("primary")
        move_row.addWidget(self.move_input, 1)
        move_row.addWidget(self.move_button)
        panel_layout.addLayout(move_row)

        actions = QGridLayout()
        self.home_button = QPushButton("Set Home")
        self.reset_button = QPushButton("Reset Board")
        self.status_button = QPushButton("Status")
        self.selftest_button = QPushButton("Self-Test")
        self.servo_up_button = QPushButton("Servo Up")
        self.servo_down_button = QPushButton("Servo Down")
        self.disable_button = QPushButton("Disable")
        self.abort_button = QPushButton("Abort")
        self.abort_button.setObjectName("danger")
        buttons = (
            self.home_button,
            self.reset_button,
            self.status_button,
            self.selftest_button,
            self.servo_up_button,
            self.servo_down_button,
            self.disable_button,
            self.abort_button,
        )
        for index, button in enumerate(buttons):
            actions.addWidget(button, index // 2, index % 2)
        panel_layout.addLayout(actions)

        self.notice = QLabel("Connect the controller to begin.")
        self.notice.setObjectName("notice")
        self.notice.setWordWrap(True)
        panel_layout.addWidget(self.notice)
        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.setMinimumHeight(110)
        self.log_view.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        panel_layout.addWidget(self.log_view, 1)
        splitter.addWidget(panel)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 0)
        splitter.setSizes([760, 380])

        abort_action = QAction("Abort Motion", self)
        abort_action.setShortcut(QKeySequence(Qt.Key.Key_Backspace))
        abort_action.triggered.connect(self._abort)
        self.addAction(abort_action)

        self.setStyleSheet("""
            QMainWindow, QWidget { background: #f3f1e8; color: #17211b; font: 14px 'DejaVu Sans'; }
            QLabel#title { font: bold 34px Georgia; }
            QLabel#connection { color: #59645c; font-weight: bold; }
            QLabel#notice { background: #e5e9df; border-left: 4px solid #2f704f; padding: 10px; }
            QLineEdit { background: #fffef8; border: 1px solid #a9b0a5; border-radius: 4px; padding: 8px; min-height: 24px; }
            QPushButton { background: #f8f8f2; border: 1px solid #637067; border-radius: 4px; padding: 6px; font-weight: bold; }
            QPushButton:hover { background: white; }
            QPushButton:disabled { color: #8b918c; border-color: #b8bdb9; }
            QPushButton#primary { background: #2f704f; color: white; border-color: #2f704f; }
            QPushButton#primary:disabled { background: #a8b7ad; color: #edf1ee; border-color: #a8b7ad; }
            QPushButton#danger { background: #b7352d; color: white; border-color: #b7352d; }
            QTextEdit { background: #18231c; color: #dce7de; border: 0; font: 12px 'DejaVu Sans Mono'; }
        """)

    def _status_item(self, layout: QGridLayout, column: int, label: str) -> QLabel:
        box = QWidget()
        box.setStyleSheet("background: #e5e9df; padding: 5px;")
        box_layout = QVBoxLayout(box)
        box_layout.setContentsMargins(5, 4, 5, 4)
        caption = QLabel(label)
        caption.setStyleSheet("color: #59645c; font-size: 11px;")
        value = QLabel("No")
        value.setStyleSheet("font-weight: bold;")
        box_layout.addWidget(caption)
        box_layout.addWidget(value)
        layout.addWidget(box, 0, column)
        return value

    def _connect_signals(self) -> None:
        self.connect_button.clicked.connect(self._toggle_connection)
        self.move_button.clicked.connect(self._move_from_input)
        self.move_input.returnPressed.connect(self._move_from_input)
        self.board_widget.square_clicked.connect(self._square_clicked)
        self.home_button.clicked.connect(self._home)
        self.reset_button.clicked.connect(self._reset)
        self.status_button.clicked.connect(lambda: self._send("status", timeout=5.0))
        self.selftest_button.clicked.connect(lambda: self._send("selftest", timeout=10.0))
        self.servo_up_button.clicked.connect(lambda: self._send("actuator_retract", timeout=5.0))
        self.servo_down_button.clicked.connect(lambda: self._send("actuator_extend", timeout=5.0))
        self.disable_button.clicked.connect(lambda: self._send("DISABLE", timeout=5.0))
        self.abort_button.clicked.connect(self._abort)
        self.serial.connection_changed.connect(self._connection_changed)
        self.serial.line_received.connect(self._log)
        self.serial.status_changed.connect(self._status_changed)
        self.serial.command_finished.connect(self._command_finished)
        self.serial.command_failed.connect(self._command_failed)

    def _toggle_connection(self) -> None:
        try:
            if self.serial.connected:
                self.serial.disconnect()
            else:
                self.serial.connect_port(self.port_input.text().strip())
                self._log(f"* connected to {self.serial.port}")
                QTimer.singleShot(2200, lambda: self._send("status", timeout=5.0))
        except RuntimeError as error:
            self._show_error(str(error))

    def _connection_changed(self, connected: bool, port: str) -> None:
        self.connection_label.setText(f"Connected · {port}" if connected else "Disconnected")
        self.connect_button.setText("Disconnect" if connected else "Connect")
        self.port_input.setEnabled(not connected)
        self._set_connected(connected)

    def _set_connected(self, connected: bool) -> None:
        controls = (
            self.home_button,
            self.reset_button,
            self.status_button,
            self.selftest_button,
            self.servo_up_button,
            self.servo_down_button,
            self.disable_button,
        )
        for button in controls:
            button.setEnabled(connected and not self.serial.pending_command)
        self.abort_button.setEnabled(connected)
        self._update_move_enabled()

    def _status_changed(self, status: dict[str, str]) -> None:
        self.home_status.setText("Set" if status.get("homed") == "1" else "No")
        mapping_seen = all(key in status for key in EXPECTED_MAPPING)
        mapping_ok = mapping_seen and all(status.get(key) == value for key, value in EXPECTED_MAPPING.items())
        if mapping_seen and not mapping_ok:
            self.board_status.setText("Mapping mismatch")
        else:
            self.board_status.setText("Ready" if status.get("board_confirmed") == "1" else "No")
        self._update_move_enabled()

    def _update_move_enabled(self) -> None:
        mapping_ok = all(self.serial.status.get(key) == value for key, value in EXPECTED_MAPPING.items())
        ready = self.serial.connected and not self.serial.pending_command and mapping_ok and self.serial.status.get("homed") == "1" and self.serial.status.get("board_confirmed") == "1"
        self.move_button.setEnabled(ready)
        self.move_input.setEnabled(ready)

    def _send(self, command: str, success: Callable[[], None] | None = None, timeout: float = 90.0) -> None:
        try:
            self.pending_success = success
            self.serial.send_async(command, timeout)
            self.command_status.setText(command)
            self.connect_button.setEnabled(False)
            self._set_connected(self.serial.connected)
        except RuntimeError as error:
            self.pending_success = None
            self._show_error(str(error))

    def _command_finished(self, command: str, _response: str) -> None:
        del _response
        callback = self.pending_success
        self.pending_success = None
        if callback:
            callback()
        self.command_status.setText("Idle")
        self.connect_button.setEnabled(True)
        self._set_connected(self.serial.connected)
        self.notice.setText(f"{command} completed.")

    def _command_failed(self, _command: str, message: str) -> None:
        del _command
        self.pending_success = None
        self.command_status.setText("Idle")
        self.connect_button.setEnabled(True)
        self._set_connected(self.serial.connected)
        self._show_error(message)

    def _home(self) -> None:
        answer = QMessageBox.question(self, "Set manual origin", "Is the carriage exactly at the a1 center?")
        if answer == QMessageBox.StandardButton.Yes:
            self._send("HOME", success=lambda: self._send("status", timeout=5.0), timeout=5.0)

    def _reset(self) -> None:
        answer = QMessageBox.question(self, "Reset board", "Does the physical board match the standard starting position?")
        if answer == QMessageBox.StandardButton.Yes:
            def reset_complete() -> None:
                self.board.reset()
                self._render_board()
                self._send("status", timeout=5.0)
            self._send("RESETBOARD", success=reset_complete, timeout=5.0)

    def _move_from_input(self) -> None:
        text = self.move_input.text().strip()
        if not text:
            return
        try:
            move = parse_white_move(self.board, text)
        except ValueError as error:
            self._show_error(str(error))
            return
        self._execute_move(move)

    def _square_clicked(self, square_name: str) -> None:
        if not self.move_button.isEnabled():
            return
        piece = self.board.piece_at(chess.parse_square(square_name))
        selected = self.board_widget.selected
        if selected is None or (piece and piece.color == chess.WHITE):
            if piece and piece.color == chess.WHITE:
                self.board_widget.selected = square_name
                self.board_widget.targets = {
                    chess.square_name(move.to_square)
                    for move in executable_moves(self.board)
                    if chess.square_name(move.from_square) == square_name
                }
                self.board_widget.update()
            return
        if square_name not in self.board_widget.targets:
            self.board_widget.selected = None
            self.board_widget.targets.clear()
            self.board_widget.update()
            self._show_error("That destination is not an executable white move")
            return
        move = chess.Move.from_uci(selected + square_name)
        self.board_widget.selected = None
        self.board_widget.targets.clear()
        self._execute_move(move)

    def _execute_move(self, move: chess.Move) -> None:
        def move_complete() -> None:
            self.board.push(move)
            self.board.turn = chess.WHITE
            self.move_input.clear()
            self._render_board()
        self._send(move.uci(), success=move_complete)

    def _render_board(self) -> None:
        self.board.turn = chess.WHITE
        self.board_widget.board = self.board.copy(stack=False)
        self.board_widget.update()

    def _abort(self) -> None:
        try:
            self.serial.abort()
            self.notice.setText("Abort sent. Re-home and reset the board before movement.")
        except RuntimeError as error:
            self._show_error(str(error))

    def _jog(self, axis: str, direction: str) -> None:
        self._send(f"jog{axis}{direction}{self.jog_mm}")

    def _toggle_jog_distance(self) -> None:
        self.jog_mm = 1 if self.jog_mm == 10 else 10
        self.notice.setText(f"Manual jog distance set to {self.jog_mm} mm.")

    def _goto_square(self) -> None:
        square, accepted = QInputDialog.getText(self, "Move carriage", "Square (for example e3):")
        square = square.strip().lower()
        if not accepted:
            return
        if square not in chess.SQUARE_NAMES:
            self._show_error("Enter a square from a1 to h8")
            return
        self._send(f"goto {square}")

    def keyPressEvent(self, event) -> None:
        if isinstance(self.focusWidget(), (QLineEdit, QTextEdit)):
            super().keyPressEvent(event)
            return

        key = event.key()
        modifiers = event.modifiers()
        if modifiers == Qt.KeyboardModifier.ShiftModifier and key == Qt.Key.Key_Right:
            self._jog("x", "+")
        elif modifiers == Qt.KeyboardModifier.ShiftModifier and key == Qt.Key.Key_Left:
            self._jog("x", "-")
        elif modifiers == Qt.KeyboardModifier.ShiftModifier and key == Qt.Key.Key_Up:
            self._jog("y", "+")
        elif modifiers == Qt.KeyboardModifier.ShiftModifier and key == Qt.Key.Key_Down:
            self._jog("y", "-")
        elif key == Qt.Key.Key_C:
            self._toggle_jog_distance()
        elif key == Qt.Key.Key_H:
            self._home()
        elif key == Qt.Key.Key_0:
            self._send("actuator_retract", timeout=5.0)
        elif key == Qt.Key.Key_9:
            self._send("actuator_extend", timeout=5.0)
        elif key == Qt.Key.Key_M:
            self.move_input.setFocus()
        elif key == Qt.Key.Key_G:
            self._goto_square()
        elif key == Qt.Key.Key_S:
            self._send("status", timeout=5.0)
        elif key == Qt.Key.Key_T:
            self._send("selftest", timeout=10.0)
        elif key == Qt.Key.Key_R:
            self._reset()
        elif key == Qt.Key.Key_D:
            self._send("DISABLE", timeout=5.0)
        elif key == Qt.Key.Key_Q:
            self.close()
        else:
            super().keyPressEvent(event)

    def _log(self, line: str) -> None:
        prefix = "" if line.startswith(">") else "< "
        self.logs.append(prefix + line)
        self.log_view.setPlainText("\n".join(self.logs))
        self.log_view.verticalScrollBar().setValue(self.log_view.verticalScrollBar().maximum())

    def _show_error(self, message: str) -> None:
        self.notice.setText(message)
        self.notice.setStyleSheet("border-left-color: #b7352d;")

    def closeEvent(self, event) -> None:
        if self.serial.pending_command:
            try:
                self.serial.abort()
            except RuntimeError:
                pass
        self.serial.disconnect(force=True)
        event.accept()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="OpenMove native board controller")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Arduino serial port")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    app = QApplication([])
    app.setApplicationName("OpenMove")
    window = OpenMoveWindow(args.port)
    available = app.primaryScreen().availableGeometry()
    window.resize(min(1180, int(available.width() * 0.94)),
                  min(700, int(available.height() * 0.90)))
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
