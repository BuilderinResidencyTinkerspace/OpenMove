#include <Servo.h>
#include <math.h>
#include <string.h>

// Confirmed CNC Shield V3 wiring.
const uint8_t A_STEP_PIN = 2;
const uint8_t B_STEP_PIN = 3;
const uint8_t A_DIR_PIN = 5;
const uint8_t B_DIR_PIN = 6;
const uint8_t ENABLE_PIN = 8;  // Active LOW.
const uint8_t ACTUATOR_PIN = 11;  // CNC Shield Z+ header; PWM signal only.

// Confirmed mechanics: 1.8 degree motors, GT2 belt, 20-tooth pulley,
// and DRV8825 drivers configured for 1/8 microstepping.
const float MOTOR_STEPS_PER_MM = 40.0f;
const float BOARD_SIZE_MM = 350.0f;
const float SQUARE_SIZE_MM = BOARD_SIZE_MM / 8.0f;
const float HALF_SQUARE_MM = SQUARE_SIZE_MM / 2.0f;
// a1 is (0, 0). The 8×8 grid ends at the h8 centre (306.25 mm); the measured
// 350 mm playing area includes the final half-square beyond that centre.
const float MAX_X_MM = BOARD_SIZE_MM;
const float MAX_Y_MM = BOARD_SIZE_MM;
// Change only after measuring the physical jog directions.
const bool INVERT_A_DIR = true;
const bool INVERT_B_DIR = true;
const bool SWAP_X_Y = false;

// Capture drop: centre of the half-cell playable area immediately right of h5.
const float GRAVEYARD_X_MM = 7.0f * SQUARE_SIZE_MM + HALF_SQUARE_MM;
const float GRAVEYARD_Y_MM = 4.0f * SQUARE_SIZE_MM;
const float GRAVEYARD_APPROACH_Y_MM = GRAVEYARD_Y_MM + HALF_SQUARE_MM;

// MG90S with a custom 3D-printed linear mechanism. These are pulse widths.
// User confirmed surface contact at 2100 us on 2026-09-17; 1000 us was clear
// of the surface. Contact is visually calibrated, not sensed by the controller.
const int ACTUATOR_RETRACT_US = 1000; // Magnet raised / piece released.
const int ACTUATOR_EXTEND_US = 2100; // Confirmed surface contact.
const int ACTUATOR_MIN_US = ACTUATOR_RETRACT_US;
const int ACTUATOR_MAX_US = ACTUATOR_EXTEND_US;
// Direct endpoint commands let the servo move at its native maximum speed.
// Rated unloaded travel is ~300 ms over 180 degrees at 4.8 V. Allow 500 ms
// before XY motion for the printed mechanism/load; this is not position feedback.
const unsigned long ACTUATOR_SETTLE_MS = 500;

// Conservative initial motion values. These require physical validation.
const float START_STEP_RATE = 80.0f;
const float MAX_STEP_RATE = 2500.0f;
const float STEP_ACCELERATION = 6000.0f;
const unsigned int STEP_HIGH_US = 10;
// Mid-square lanes are not safe until measured against the largest piece base.
const bool KNIGHT_LANE_ROUTE_VALIDATED = false;

Servo magnetActuator;

long motorAPosition = 0;
long motorBPosition = 0;
float carriageX = 0.0f;
float carriageY = 0.0f;
bool manuallyHomed = false;
bool motionAborted = false;
bool emergencyStopLatched = false;
bool emergencyDuringCommand = false;
int actuatorPulseUs = ACTUATOR_RETRACT_US;
bool actuatorSettled = false; // True only after the travel wait completes.

char board[8][8];
char inputLine[32];
uint8_t inputLength = 0;
bool discardLine = false;
bool boardTrusted = false;
bool whiteToMove = true;

void stopController() {
  digitalWrite(ENABLE_PIN, HIGH);
  magnetActuator.writeMicroseconds(ACTUATOR_RETRACT_US);
  actuatorPulseUs = ACTUATOR_RETRACT_US;
  actuatorSettled = false;
  manuallyHomed = false;
  boardTrusted = false;
  motionAborted = true;
  emergencyStopLatched = true;
  emergencyDuringCommand = true;
  inputLength = 0;
  discardLine = true;
}

bool emergencyRequested();

void settleActuator() {
  unsigned long started = millis();
  while (millis() - started < ACTUATOR_SETTLE_MS) {
    if (emergencyRequested()) { stopController(); return; }
    delay(1);
  }
  actuatorSettled = true;
}

void resetBoardState() {
  const char backRankWhite[8] = {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'};
  const char backRankBlack[8] = {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'};

  for (uint8_t file = 0; file < 8; file++) {
    board[file][0] = backRankWhite[file];
    board[file][1] = 'P';
    for (uint8_t rank = 2; rank < 6; rank++) board[file][rank] = 0;
    board[file][6] = 'p';
    board[file][7] = backRankBlack[file];
  }
  whiteToMove = true;
}

void setActuatorPosition(int pulseUs) {
  if (motionAborted) return;
  if (actuatorSettled && actuatorPulseUs == pulseUs) return;
  actuatorSettled = false;
  magnetActuator.writeMicroseconds(pulseUs);
  actuatorPulseUs = pulseUs;
  settleActuator();
}

void releaseMagnet() {
  setActuatorPosition(ACTUATOR_RETRACT_US);
}

void engageMagnet() {
  setActuatorPosition(ACTUATOR_EXTEND_US);
}

void waitMicros(unsigned long duration) {
  if (duration >= 1000UL) {
    delay(duration / 1000UL);
    duration %= 1000UL;
  }
  if (duration) delayMicroseconds((unsigned int)duration);
}

bool emergencyRequested() {
  while (Serial.available()) {
    int incoming = Serial.read();
    if (incoming == '!' || incoming == 0x18) return true;
    // Commands received during execution are rejected, never partly executed.
    if (incoming == '\n' || incoming == '\r') {
      if (discardLine) Serial.println(F("warning:command-discarded-while-busy"));
      discardLine = false;
    } else discardLine = true;
  }
  return false;
}

void setDirection(uint8_t pin, long delta) {
  bool inverted = pin == A_DIR_PIN ? INVERT_A_DIR : INVERT_B_DIR;
  digitalWrite(pin, ((delta >= 0) != inverted) ? HIGH : LOW);
}

bool moveMotors(long targetA, long targetB) {
  if (motionAborted || !manuallyHomed) return false;
  long deltaA = targetA - motorAPosition;
  long deltaB = targetB - motorBPosition;
  unsigned long stepsA = labs(deltaA);
  unsigned long stepsB = labs(deltaB);
  unsigned long totalEvents = max(stepsA, stepsB);

  if (totalEvents == 0) return true;

  setDirection(A_DIR_PIN, deltaA);
  setDirection(B_DIR_PIN, deltaB);
  delayMicroseconds(20);

  unsigned long errorA = totalEvents / 2;
  unsigned long errorB = totalEvents / 2;

  for (unsigned long event = 0; event < totalEvents; event++) {
    if (emergencyRequested()) {
      stopController();
      return false;
    }

    bool stepA = false;
    bool stepB = false;
    errorA += stepsA;
    errorB += stepsB;
    if (errorA >= totalEvents) {
      errorA -= totalEvents;
      stepA = true;
    }
    if (errorB >= totalEvents) {
      errorB -= totalEvents;
      stepB = true;
    }

    unsigned long fromEnd = min(event, totalEvents - 1UL - event);
    float rate = sqrt(START_STEP_RATE * START_STEP_RATE +
                      2.0f * STEP_ACCELERATION * (float)fromEnd);
    if (rate > MAX_STEP_RATE) rate = MAX_STEP_RATE;
    unsigned long period = (unsigned long)(1000000.0f / rate);

    if (stepA) digitalWrite(A_STEP_PIN, HIGH);
    if (stepB) digitalWrite(B_STEP_PIN, HIGH);
    delayMicroseconds(STEP_HIGH_US);
    if (stepA) digitalWrite(A_STEP_PIN, LOW);
    if (stepB) digitalWrite(B_STEP_PIN, LOW);
    waitMicros(period > STEP_HIGH_US ? period - STEP_HIGH_US : 1UL);

    if (stepA) motorAPosition += deltaA >= 0 ? 1 : -1;
    if (stepB) motorBPosition += deltaB >= 0 ? 1 : -1;
  }
  return true;
}

bool moveTo(float x, float y) {
  if (x < 0.0f || x > MAX_X_MM || y < 0.0f || y > MAX_Y_MM) {
    Serial.println(F("error:target-outside-configured-workspace"));
    return false;
  }

  // Selected H-bot mapping. Both physical directions and scale need measurement.
  float mappedX = SWAP_X_Y ? y : x;
  float mappedY = SWAP_X_Y ? x : y;
  long targetA = lroundf((mappedX + mappedY) * MOTOR_STEPS_PER_MM);
  long targetB = lroundf((mappedY - mappedX) * MOTOR_STEPS_PER_MM);
  if (!moveMotors(targetA, targetB)) return false;
  carriageX = x;
  carriageY = y;
  return true;
}

// The manually established origin is the centre of a1.
float squareX(uint8_t file) { return file * SQUARE_SIZE_MM; }
float squareY(uint8_t rank) { return rank * SQUARE_SIZE_MM; }

float departureLaneY(uint8_t sourceRank, uint8_t destinationRank) {
  if (destinationRank > sourceRank) return squareY(sourceRank) + HALF_SQUARE_MM;
  if (destinationRank < sourceRank) return squareY(sourceRank) - HALF_SQUARE_MM;
  if (sourceRank == 7) return squareY(sourceRank) - HALF_SQUARE_MM;
  return squareY(sourceRank) + HALF_SQUARE_MM;
}

float arrivalLaneY(uint8_t sourceRank, uint8_t destinationRank) {
  if (destinationRank > sourceRank) return squareY(destinationRank) - HALF_SQUARE_MM;
  if (destinationRank < sourceRank) return squareY(destinationRank) + HALF_SQUARE_MM;
  return departureLaneY(sourceRank, destinationRank);
}

float safeVerticalLane(uint8_t sourceFile, uint8_t destinationFile) {
  if (sourceFile == 7) return squareX(sourceFile) - HALF_SQUARE_MM;
  if (destinationFile >= sourceFile) return squareX(sourceFile) + HALF_SQUARE_MM;
  return squareX(sourceFile) - HALF_SQUARE_MM;
}

bool carryBetweenSquares(uint8_t sourceFile, uint8_t sourceRank,
                         uint8_t destinationFile, uint8_t destinationRank) {
  int fileDistance = abs((int)destinationFile - (int)sourceFile);
  int rankDistance = abs((int)destinationRank - (int)sourceRank);
  bool straightMove = sourceFile == destinationFile || sourceRank == destinationRank;
  bool diagonalMove = fileDistance == rankDistance;
  if (straightMove || diagonalMove) {
    return moveTo(squareX(destinationFile), squareY(destinationRank));
  }

  float sourceX = squareX(sourceFile);
  float sourceLaneY = departureLaneY(sourceRank, destinationRank);
  float laneX = safeVerticalLane(sourceFile, destinationFile);
  float destinationLaneY = arrivalLaneY(sourceRank, destinationRank);
  float destinationX = squareX(destinationFile);

  return moveTo(sourceX, sourceLaneY) &&
         moveTo(laneX, sourceLaneY) &&
         moveTo(laneX, destinationLaneY) &&
         moveTo(destinationX, destinationLaneY) &&
         moveTo(destinationX, squareY(destinationRank));
}

bool pickupSquare(uint8_t file, uint8_t rank) {
  releaseMagnet();
  if (!moveTo(squareX(file), squareY(rank))) return false;
  engageMagnet();
  return !motionAborted;
}

bool removePieceToGraveyard(uint8_t file, uint8_t rank) {
  if (!pickupSquare(file, rank)) return false;

  // Enter an internal lane, approach the right edge between ranks 5 and 6,
  // then slide along the edge to the requested position outside h5.
  uint8_t graveyardRank = 4;
  float sourceLaneY = departureLaneY(rank, graveyardRank);
  float internalRightLaneX = squareX(7) - HALF_SQUARE_MM;
  if (!moveTo(squareX(file), sourceLaneY) ||
      !moveTo(internalRightLaneX, sourceLaneY) ||
      !moveTo(internalRightLaneX, GRAVEYARD_APPROACH_Y_MM) ||
      !moveTo(GRAVEYARD_X_MM, GRAVEYARD_APPROACH_Y_MM) ||
      !moveTo(GRAVEYARD_X_MM, GRAVEYARD_Y_MM)) return false;

  releaseMagnet();
  Serial.println(F("warning:graveyard-drop-not-sensor-verified"));
  return !motionAborted;
}

bool mechanicallyMovePiece(uint8_t sourceFile, uint8_t sourceRank,
                           uint8_t destinationFile, uint8_t destinationRank) {
  if (!pickupSquare(sourceFile, sourceRank)) return false;
  if (!carryBetweenSquares(sourceFile, sourceRank, destinationFile, destinationRank)) {
    releaseMagnet();
    return false;
  }
  releaseMagnet();
  return !motionAborted;
}

bool isWhite(char piece) { return piece >= 'A' && piece <= 'Z'; }
bool isBlack(char piece) { return piece >= 'a' && piece <= 'z'; }

bool pathIsClear(uint8_t sourceFile, uint8_t sourceRank,
                 uint8_t destinationFile, uint8_t destinationRank) {
  int fileStep = destinationFile > sourceFile ? 1 : destinationFile < sourceFile ? -1 : 0;
  int rankStep = destinationRank > sourceRank ? 1 : destinationRank < sourceRank ? -1 : 0;
  int file = (int)sourceFile + fileStep;
  int rank = (int)sourceRank + rankStep;

  while (file != destinationFile || rank != destinationRank) {
    if (board[file][rank]) return false;
    file += fileStep;
    rank += rankStep;
  }
  return true;
}

bool pieceAttacksSquare(char piece, uint8_t sourceFile, uint8_t sourceRank,
                        uint8_t destinationFile, uint8_t destinationRank) {
  int fileDelta = (int)destinationFile - (int)sourceFile;
  int rankDelta = (int)destinationRank - (int)sourceRank;
  int absoluteFileDelta = abs(fileDelta);
  int absoluteRankDelta = abs(rankDelta);
  char pieceType = isWhite(piece) ? piece + ('a' - 'A') : piece;

  if (pieceType == 'p') {
    int direction = isWhite(piece) ? 1 : -1;
    return absoluteFileDelta == 1 && rankDelta == direction;
  }
  if (pieceType == 'n') {
    return (absoluteFileDelta == 1 && absoluteRankDelta == 2) ||
           (absoluteFileDelta == 2 && absoluteRankDelta == 1);
  }
  if (pieceType == 'k') return max(absoluteFileDelta, absoluteRankDelta) == 1;
  if (pieceType == 'b') {
    return absoluteFileDelta == absoluteRankDelta &&
           pathIsClear(sourceFile, sourceRank, destinationFile, destinationRank);
  }
  if (pieceType == 'r') {
    return (fileDelta == 0 || rankDelta == 0) &&
           pathIsClear(sourceFile, sourceRank, destinationFile, destinationRank);
  }
  if (pieceType == 'q') {
    bool straight = fileDelta == 0 || rankDelta == 0;
    bool diagonal = absoluteFileDelta == absoluteRankDelta;
    return (straight || diagonal) &&
           pathIsClear(sourceFile, sourceRank, destinationFile, destinationRank);
  }
  return false;
}

bool pieceMoveIsValid(char piece, uint8_t sourceFile, uint8_t sourceRank,
                      uint8_t destinationFile, uint8_t destinationRank) {
  char destinationPiece = board[destinationFile][destinationRank];
  char pieceType = isWhite(piece) ? piece + ('a' - 'A') : piece;
  int fileDelta = (int)destinationFile - (int)sourceFile;
  int rankDelta = (int)destinationRank - (int)sourceRank;

  if (pieceType != 'p') {
    return pieceAttacksSquare(piece, sourceFile, sourceRank, destinationFile, destinationRank);
  }

  int direction = isWhite(piece) ? 1 : -1;
  uint8_t startingRank = isWhite(piece) ? 1 : 6;
  if (fileDelta == 0 && !destinationPiece) {
    if (rankDelta == direction) return true;
    if (sourceRank == startingRank && rankDelta == 2 * direction) {
      return !board[sourceFile][sourceRank + direction];
    }
  }
  return abs(fileDelta) == 1 && rankDelta == direction && destinationPiece;
}

bool moveLeavesKingInCheck(char movingPiece, uint8_t sourceFile, uint8_t sourceRank,
                           uint8_t destinationFile, uint8_t destinationRank) {
  char destinationPiece = board[destinationFile][destinationRank];
  board[sourceFile][sourceRank] = 0;
  board[destinationFile][destinationRank] = movingPiece;

  char king = isWhite(movingPiece) ? 'K' : 'k';
  uint8_t kingFile = 0;
  uint8_t kingRank = 0;
  bool kingFound = false;
  for (uint8_t file = 0; file < 8 && !kingFound; file++) {
    for (uint8_t rank = 0; rank < 8; rank++) {
      if (board[file][rank] == king) {
        kingFile = file;
        kingRank = rank;
        kingFound = true;
        break;
      }
    }
  }

  bool attacked = !kingFound;
  for (uint8_t file = 0; file < 8 && !attacked; file++) {
    for (uint8_t rank = 0; rank < 8; rank++) {
      char attacker = board[file][rank];
      if (attacker && isWhite(attacker) != isWhite(movingPiece) &&
          pieceAttacksSquare(attacker, file, rank, kingFile, kingRank)) {
        attacked = true;
        break;
      }
    }
  }

  board[sourceFile][sourceRank] = movingPiece;
  board[destinationFile][destinationRank] = destinationPiece;
  return attacked;
}

bool parseSquare(const char *text, uint8_t &file, uint8_t &rank) {
  if (text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8') return false;
  file = text[0] - 'a';
  rank = text[1] - '1';
  return true;
}

char lowerAscii(char value) {
  return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

bool normalizeMoveNotation(const char *notation, char *coordinateMove) {
  size_t length = strlen(notation);
  if (length == 4 || length == 5) {
    coordinateMove[0] = lowerAscii(notation[0]);
    coordinateMove[1] = notation[1];
    coordinateMove[2] = lowerAscii(notation[2]);
    coordinateMove[3] = notation[3];
    coordinateMove[4] = length == 5 ? lowerAscii(notation[4]) : 0;
    coordinateMove[5] = 0;
    uint8_t ignoredFile, ignoredRank;
    if (parseSquare(coordinateMove, ignoredFile, ignoredRank) &&
        parseSquare(coordinateMove + 2, ignoredFile, ignoredRank)) return true;
  }

  if (length && (notation[length - 1] == '+' || notation[length - 1] == '#')) return false;
  char promotion = 0;
  if (length >= 2 && notation[length - 2] == '=') {
    promotion = lowerAscii(notation[length - 1]);
    length -= 2;
  }
  if (length < 2) return false;

  char destinationText[3] = {lowerAscii(notation[length - 2]), notation[length - 1], 0};
  uint8_t destinationFile, destinationRank;
  if (!parseSquare(destinationText, destinationFile, destinationRank)) return false;

  char requestedType = 'P';
  size_t index = 0;
  if (notation[0] == 'K' || notation[0] == 'Q' || notation[0] == 'R' ||
      notation[0] == 'B' || notation[0] == 'N') {
    requestedType = notation[0];
    index = 1;
  }
  size_t qualifierStart = index;

  int requestedFile = -1;
  int requestedRank = -1;
  bool captureMarked = false;
  for (; index < length - 2; index++) {
    char symbol = notation[index];
    if (symbol == 'x' || symbol == 'X') captureMarked = true;
    else if (symbol >= 'a' && symbol <= 'h' && requestedFile < 0) requestedFile = symbol - 'a';
    else if (symbol >= '1' && symbol <= '8' && requestedRank < 0) requestedRank = symbol - '1';
    else return false;
  }

  size_t qualifierLength = length - 2 - qualifierStart;
  if (requestedType == 'P') {
    if (captureMarked) {
      if (qualifierLength != 2 || requestedFile < 0 || requestedRank >= 0) return false;
    } else if (qualifierLength != 0 || requestedFile >= 0 || requestedRank >= 0) {
      return false;
    }
  }

  bool destinationOccupied = board[destinationFile][destinationRank] != 0;
  if (captureMarked != destinationOccupied) return false;

  char boardPiece = whiteToMove ? requestedType : lowerAscii(requestedType);
  uint8_t sourceFile = 0;
  uint8_t sourceRank = 0;
  uint8_t candidates = 0;
  for (uint8_t file = 0; file < 8; file++) {
    for (uint8_t rank = 0; rank < 8; rank++) {
      if (board[file][rank] != boardPiece ||
          (requestedFile >= 0 && requestedFile != file) ||
          (requestedRank >= 0 && requestedRank != rank)) continue;
      if (pieceMoveIsValid(boardPiece, file, rank, destinationFile, destinationRank) &&
          !moveLeavesKingInCheck(boardPiece, file, rank, destinationFile, destinationRank)) {
        sourceFile = file;
        sourceRank = rank;
        candidates++;
      }
    }
  }
  if (candidates != 1) return false;

  coordinateMove[0] = 'a' + sourceFile;
  coordinateMove[1] = '1' + sourceRank;
  coordinateMove[2] = 'a' + destinationFile;
  coordinateMove[3] = '1' + destinationRank;
  coordinateMove[4] = promotion;
  coordinateMove[5] = 0;
  return true;
}

void runSelfTest() {
  char savedBoard[8][8];
  memcpy(savedBoard, board, sizeof(board));
  bool savedWhiteToMove = whiteToMove;
  resetBoardState();

  char coordinateMove[6];
  bool passed = normalizeMoveNotation("e4", coordinateMove) &&
                !strcmp(coordinateMove, "e2e4") &&
                normalizeMoveNotation("Nf3", coordinateMove) &&
                !strcmp(coordinateMove, "g1f3") &&
                !normalizeMoveNotation("Nxe5", coordinateMove) &&
                !normalizeMoveNotation("xd5", coordinateMove) &&
                !normalizeMoveNotation("ed5", coordinateMove) &&
                !normalizeMoveNotation("e4+", coordinateMove) &&
                fabs(GRAVEYARD_X_MM - 328.125f) < 0.01f &&
                fabs(GRAVEYARD_Y_MM - 175.0f) < 0.01f;

  memcpy(board, savedBoard, sizeof(board));
  whiteToMove = savedWhiteToMove;
  Serial.println(passed ? F("ok:selftest-passed; no-motion") : F("error:selftest-failed; no-motion"));
}

void rejectChessMove(const __FlashStringHelper *message) {
  Serial.println(message);
  if (!whiteToMove && boardTrusted) {
    boardTrusted = false;
    Serial.println(F("warning:rejected-black-entry; physical-board-state-untrusted"));
  }
}

void executeChessMove(const char *moveText) {
  if (!boardTrusted) {
    rejectChessMove(F("error:confirm-standard-physical-board-with-RESETBOARD"));
    return;
  }
  if (!manuallyHomed) {
    rejectChessMove(F("error:not-homed; place carriage at a1 centre and send HOME"));
    return;
  }

  char coordinateMove[6];
  if (!normalizeMoveNotation(moveText, coordinateMove)) {
    rejectChessMove(F("error:invalid-or-ambiguous-move; use e2e4, Nf3, or Nxe5"));
    return;
  }
  moveText = coordinateMove;

  size_t length = strlen(moveText);
  if (length != 4 && length != 5) {
    rejectChessMove(F("error:use coordinate move such as e2e4 or e7e8q"));
    return;
  }

  uint8_t sourceFile, sourceRank, destinationFile, destinationRank;
  if (!parseSquare(moveText, sourceFile, sourceRank) ||
      !parseSquare(moveText + 2, destinationFile, destinationRank)) {
    rejectChessMove(F("error:invalid-square"));
    return;
  }

  char movingPiece = board[sourceFile][sourceRank];
  char destinationPiece = board[destinationFile][destinationRank];
  if (!movingPiece) {
    rejectChessMove(F("error:source-square-empty-in-internal-board-state"));
    return;
  }
  if (destinationPiece &&
      ((isWhite(movingPiece) && isWhite(destinationPiece)) ||
       (isBlack(movingPiece) && isBlack(destinationPiece)))) {
    rejectChessMove(F("error:destination-has-same-colour-piece"));
    return;
  }
  if (isWhite(movingPiece) != whiteToMove) {
    rejectChessMove(F("error:wrong-side-to-move"));
    return;
  }
  if (destinationPiece == 'K' || destinationPiece == 'k') {
    rejectChessMove(F("error:capturing-a-king-is-not-a-legal-chess-move"));
    return;
  }

  char promotion = 0;
  bool lastRank = (movingPiece == 'P' && destinationRank == 7) ||
                  (movingPiece == 'p' && destinationRank == 0);
  if (lastRank != (length == 5)) {
    rejectChessMove(F("error:promotion-suffix-required-only-on-final-rank"));
    return;
  }
  if (length == 5) {
    promotion = moveText[4];
    if (promotion >= 'A' && promotion <= 'Z') promotion += 'a' - 'A';
    if ((movingPiece != 'P' && movingPiece != 'p') ||
        (promotion != 'q' && promotion != 'r' && promotion != 'b' && promotion != 'n')) {
      rejectChessMove(F("error:invalid-promotion"));
      return;
    }
    rejectChessMove(F("error:promotion-not-validated; replace-piece-and-state-manually"));
    return;
  }

  // En passant: diagonal pawn move to an empty destination.
  bool enPassant = (movingPiece == 'P' || movingPiece == 'p') &&
                   sourceFile != destinationFile && !destinationPiece;

  // These special actions need history/physical confirmation. Reject before motion.
  if (enPassant) {
    rejectChessMove(F("error:en-passant-not-validated"));
    return;
  }
  if ((movingPiece == 'K' || movingPiece == 'k') &&
      abs((int)destinationFile - (int)sourceFile) == 2) {
    rejectChessMove(F("error:castling-not-validated"));
    return;
  }
  if (!pieceMoveIsValid(movingPiece, sourceFile, sourceRank,
                        destinationFile, destinationRank)) {
    rejectChessMove(F("error:illegal-piece-movement-or-blocked-path"));
    return;
  }
  if (moveLeavesKingInCheck(movingPiece, sourceFile, sourceRank,
                            destinationFile, destinationRank)) {
    rejectChessMove(F("error:move-leaves-king-in-check"));
    return;
  }
  if (movingPiece == 'N' && !KNIGHT_LANE_ROUTE_VALIDATED) {
    rejectChessMove(F("error:knight-lane-clearance-not-physically-validated"));
    return;
  }

  Serial.print(F("busy:"));
  Serial.println(moveText);

  if (isBlack(movingPiece)) {
    board[sourceFile][sourceRank] = 0;
    board[destinationFile][destinationRank] = movingPiece;
    whiteToMove = true;
    Serial.print(F("ok:human-black-move-recorded:"));
    Serial.println(moveText);
    return;
  }

  if (destinationPiece && !removePieceToGraveyard(destinationFile, destinationRank)) {
    boardTrusted = false;
    if (motionAborted) return;
    Serial.println(F("error:captured-piece-removal-failed; physical-state-uncertain"));
    return;
  }

  if (!mechanicallyMovePiece(sourceFile, sourceRank, destinationFile, destinationRank)) {
    boardTrusted = false;
    if (motionAborted) return;
    Serial.println(F("error:piece-move-failed; physical-state-may-be-uncertain"));
    return;
  }

  board[sourceFile][sourceRank] = 0;
  board[destinationFile][destinationRank] = movingPiece;
  whiteToMove = !whiteToMove;

  Serial.print(F("ok:"));
  Serial.println(moveText);
}

void gotoSquare(const char *squareText) {
  if (!manuallyHomed) {
    Serial.println(F("error:not-homed; run HOME first"));
    return;
  }

  uint8_t file, rank;
  if (!parseSquare(squareText, file, rank) || squareText[2] != 0) {
    Serial.println(F("error:use GOTO followed by a square, for example GOTO e3"));
    return;
  }

  releaseMagnet();
  if (motionAborted) return;
  if (moveTo(squareX(file), squareY(rank))) {
    Serial.print(F("ok:goto-"));
    Serial.println(squareText);
  }
}

void printStatus() {
  Serial.print(F("status:homed="));
  Serial.print(manuallyHomed ? 1 : 0);
  Serial.print(F(",x="));
  Serial.print(carriageX, 2);
  Serial.print(F(",y="));
  Serial.print(carriageY, 2);
  Serial.print(F(",actuator_pulse_us="));
  Serial.print(actuatorPulseUs);
  Serial.print(F(",steps_per_mm="));
  Serial.println(MOTOR_STEPS_PER_MM, 1);
  Serial.print(F("info:max_step_rate="));
  Serial.print(MAX_STEP_RATE, 1);
  Serial.print(F(",step_acceleration="));
  Serial.println(STEP_ACCELERATION, 1);
  Serial.print(F("info:board_confirmed="));
  Serial.println(boardTrusted ? 1 : 0);
  Serial.print(F("info:side_to_move="));
  Serial.println(whiteToMove ? F("white") : F("black"));
  Serial.println(F("info:motion_side=white; black_moves_are_human-recorded"));
  Serial.print(F("info:actuator_release_us="));
  Serial.print(ACTUATOR_RETRACT_US);
  Serial.print(F(",actuator_engage_us="));
  Serial.print(ACTUATOR_EXTEND_US);
  Serial.print(F(",actuator_settle_ms="));
  Serial.println(ACTUATOR_SETTLE_MS);
  Serial.print(F("info:square_mm="));
  Serial.print(SQUARE_SIZE_MM, 2);
  Serial.print(F(",invert_a="));
  Serial.print(INVERT_A_DIR ? 1 : 0);
  Serial.print(F(",invert_b="));
  Serial.print(INVERT_B_DIR ? 1 : 0);
  Serial.print(F(",swap_xy="));
  Serial.println(SWAP_X_Y ? 1 : 0);
  Serial.print(F("info:emergency_stop_latched="));
  Serial.println(emergencyStopLatched ? 1 : 0);
  Serial.print(F("info:knight_lane_route_validated="));
  Serial.println(KNIGHT_LANE_ROUTE_VALIDATED ? 1 : 0);
  Serial.println(F("ok:status"));
}

void processCommand(char *command) {
  char originalCommand[sizeof(inputLine)];
  strncpy(originalCommand, command, sizeof(originalCommand));
  originalCommand[sizeof(originalCommand) - 1] = 0;
  for (char *p = command; *p; p++) {
    if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
  }

  if (emergencyStopLatched && strcmp(command, "home") && strcmp(command, "status") &&
      strcmp(command, "selftest")) {
    Serial.println(F("error:emergency-stop-latched; place carriage at a1 centre and send HOME"));
    return;
  }

  if (!strcmp(command, "home")) {
    emergencyStopLatched = false;
    motionAborted = false;
    releaseMagnet();
    if (motionAborted) return;
    motorAPosition = 0;
    motorBPosition = 0;
    carriageX = 0.0f;
    carriageY = 0.0f;
    manuallyHomed = true;
    digitalWrite(ENABLE_PIN, LOW);
    Serial.println(F("ok:manual-home-set-at-a1-centre; motors-enabled"));
  } else if (!strcmp(command, "status")) {
    printStatus();
  } else if (!strcmp(command, "selftest")) {
    runSelfTest();
  } else if (!strcmp(command, "actuator_retract") || !strcmp(command, "magnet0")) {
    releaseMagnet();
    if (motionAborted) return;
    Serial.println(F("ok:actuator-retracted; magnet-released"));
  } else if (!strcmp(command, "actuator_extend") || !strcmp(command, "magnet90")) {
    engageMagnet();
    if (motionAborted) return;
    Serial.println(F("ok:actuator-extended; magnet-engaged"));
  } else if (!strncmp(command, "goto ", 5)) {
    gotoSquare(command + 5);
  } else if (!strcmp(command, "jogx+10") || !strcmp(command, "jogx-10") ||
             !strcmp(command, "jogy+10") || !strcmp(command, "jogy-10") ||
             !strcmp(command, "jogx+1") || !strcmp(command, "jogx-1") ||
             !strcmp(command, "jogy+1") || !strcmp(command, "jogy-1")) {
    if (!manuallyHomed) {
      Serial.println(F("error:not-homed; run HOME first"));
      return;
    }
    releaseMagnet();
    if (motionAborted) return;
    float targetX = carriageX;
    float targetY = carriageY;
    float distance = command[6] == '0' ? 10.0f : 1.0f;
    if (command[4] == '-') distance = -distance;
    if (command[3] == 'x') targetX += distance;
    else targetY += distance;
    if (moveTo(targetX, targetY)) Serial.println(F("ok:jog-complete"));
  } else if (!strcmp(command, "resetboard")) {
    resetBoardState();
    boardTrusted = true;
    Serial.println(F("ok:internal-board-reset-to-standard-starting-position"));
  } else if (!strcmp(command, "disable")) {
    digitalWrite(ENABLE_PIN, HIGH);
    manuallyHomed = false;
    if (actuatorPulseUs != ACTUATOR_RETRACT_US) boardTrusted = false;
    releaseMagnet();
    if (motionAborted) return;
    Serial.println(F("ok:motors-disabled; position-lost; run HOME before moves"));
  } else {
    executeChessMove(originalCommand);
  }
}

void setup() {
  digitalWrite(ENABLE_PIN, HIGH); // Set latch before output mode: avoid enable glitch.
  pinMode(A_STEP_PIN, OUTPUT);
  pinMode(B_STEP_PIN, OUTPUT);
  pinMode(A_DIR_PIN, OUTPUT);
  pinMode(B_DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  digitalWrite(A_STEP_PIN, LOW);
  digitalWrite(B_STEP_PIN, LOW);
  digitalWrite(ENABLE_PIN, HIGH); // Safe startup: motors disabled until manual HOME.

  Serial.begin(115200);
  magnetActuator.writeMicroseconds(ACTUATOR_RETRACT_US);
  magnetActuator.attach(ACTUATOR_PIN, ACTUATOR_MIN_US, ACTUATOR_MAX_US);
  settleActuator();
  resetBoardState();

  Serial.println(F("OpenMove chess motion controller 1.0"));
  Serial.println(F("mode:white pieces automated; move black physically, then enter its move"));
  Serial.println(F("ready:place carriage at a1 centre, then send HOME"));
}

void loop() {
  while (Serial.available()) {
    char incoming = Serial.read();
    if (incoming == '\r' || incoming == '\n') {
      if (discardLine) { discardLine = false; inputLength = 0; continue; }
      if (inputLength) {
        inputLine[inputLength] = 0;
        emergencyDuringCommand = false;
        processCommand(inputLine);
        if (emergencyDuringCommand) {
          Serial.println(F("error:emergency-stop; position-and-board-uncertain"));
        }
        inputLength = 0;
      }
    } else if (incoming == '!' || incoming == 0x18) {
      stopController();
      Serial.println(F("error:emergency-stop; position-lost; run HOME"));
    } else if (discardLine) {
      continue;
    } else if (inputLength < sizeof(inputLine) - 1) {
      inputLine[inputLength++] = incoming;
    } else {
      inputLength = 0;
      discardLine = true;
      Serial.println(F("error:command-too-long"));
    }
  }
}
