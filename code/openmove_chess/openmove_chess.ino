#include <Servo.h>
#include <math.h>
#include <string.h>

// Confirmed CNC Shield V3 wiring.
const uint8_t A_STEP_PIN = 2;
const uint8_t B_STEP_PIN = 3;
const uint8_t A_DIR_PIN = 5;
const uint8_t B_DIR_PIN = 6;
const uint8_t ENABLE_PIN = 8;  // Active LOW.
const uint8_t SERVO_PIN = 11;  // CNC Shield Z+ header.

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

// Experimental capture drop: board edge immediately outside h5.
// The user has not yet physically verified that a released piece falls here.
const float GRAVEYARD_X_MM = MAX_X_MM;
const float GRAVEYARD_Y_MM = 4.0f * SQUARE_SIZE_MM;
const float GRAVEYARD_APPROACH_Y_MM = GRAVEYARD_Y_MM + HALF_SQUARE_MM;

const uint8_t MAGNET_RELEASE_ANGLE = 0;
const uint8_t MAGNET_ENGAGE_ANGLE = 90;
const unsigned long SERVO_SETTLE_MS = 700;

// Conservative initial motion values. These require physical validation.
const float START_STEP_RATE = 80.0f;
const float MAX_STEP_RATE = 2500.0f;
const float STEP_ACCELERATION = 6000.0f;
const unsigned int STEP_HIGH_US = 10;

Servo magnetServo;

long motorAPosition = 0;
long motorBPosition = 0;
float carriageX = 0.0f;
float carriageY = 0.0f;
bool manuallyHomed = false;
bool motionAborted = false;
uint8_t magnetAngle = MAGNET_RELEASE_ANGLE;

char board[8][8];
char inputLine[32];
uint8_t inputLength = 0;
bool discardLine = false;
bool boardTrusted = false;

void stopController() {
  digitalWrite(ENABLE_PIN, HIGH);
  magnetServo.write(MAGNET_RELEASE_ANGLE);
  magnetAngle = MAGNET_RELEASE_ANGLE;
  manuallyHomed = false;
  boardTrusted = false;
  motionAborted = true;
  inputLength = 0;
  discardLine = true;
}

bool emergencyRequested();

void settleServo() {
  unsigned long started = millis();
  while (millis() - started < SERVO_SETTLE_MS) {
    if (emergencyRequested()) { stopController(); return; }
    delay(1);
  }
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
}

void releaseMagnet() {
  magnetServo.write(MAGNET_RELEASE_ANGLE);
  magnetAngle = MAGNET_RELEASE_ANGLE;
  settleServo();
}

void engageMagnet() {
  magnetServo.write(MAGNET_ENGAGE_ANGLE);
  magnetAngle = MAGNET_ENGAGE_ANGLE;
  settleServo();
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
    waitMicros(period - STEP_HIGH_US);

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

float safeHorizontalLane(uint8_t rank) {
  if (rank == 7) return squareY(rank) - HALF_SQUARE_MM;
  return squareY(rank) + HALF_SQUARE_MM;
}

float safeVerticalLane(uint8_t sourceFile, uint8_t destinationFile) {
  if (sourceFile == 7) return squareX(sourceFile) - HALF_SQUARE_MM;
  if (destinationFile >= sourceFile) return squareX(sourceFile) + HALF_SQUARE_MM;
  return squareX(sourceFile) - HALF_SQUARE_MM;
}

bool carryBetweenSquares(uint8_t sourceFile, uint8_t sourceRank,
                         uint8_t destinationFile, uint8_t destinationRank) {
  float sourceX = squareX(sourceFile);
  float sourceLaneY = safeHorizontalLane(sourceRank);
  float laneX = safeVerticalLane(sourceFile, destinationFile);
  float destinationLaneY = safeHorizontalLane(destinationRank);
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
  float sourceLaneY = safeHorizontalLane(rank);
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
  if (!carryBetweenSquares(sourceFile, sourceRank, destinationFile, destinationRank)) return false;
  releaseMagnet();
  return !motionAborted;
}

bool isWhite(char piece) { return piece >= 'A' && piece <= 'Z'; }
bool isBlack(char piece) { return piece >= 'a' && piece <= 'z'; }

bool parseSquare(const char *text, uint8_t &file, uint8_t &rank) {
  if (text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8') return false;
  file = text[0] - 'a';
  rank = text[1] - '1';
  return true;
}

void executeChessMove(const char *moveText) {
  if (!boardTrusted) {
    Serial.println(F("error:confirm-standard-physical-board-with-RESETBOARD"));
    return;
  }
  if (!manuallyHomed) {
    Serial.println(F("error:not-homed; place carriage at a1 centre and send HOME"));
    return;
  }

  size_t length = strlen(moveText);
  if (length != 4 && length != 5) {
    Serial.println(F("error:use coordinate move such as e2e4 or e7e8q"));
    return;
  }

  uint8_t sourceFile, sourceRank, destinationFile, destinationRank;
  if (!parseSquare(moveText, sourceFile, sourceRank) ||
      !parseSquare(moveText + 2, destinationFile, destinationRank)) {
    Serial.println(F("error:invalid-square"));
    return;
  }

  char movingPiece = board[sourceFile][sourceRank];
  char destinationPiece = board[destinationFile][destinationRank];
  if (!movingPiece) {
    Serial.println(F("error:source-square-empty-in-internal-board-state"));
    return;
  }
  if (destinationPiece &&
      ((isWhite(movingPiece) && isWhite(destinationPiece)) ||
       (isBlack(movingPiece) && isBlack(destinationPiece)))) {
    Serial.println(F("error:destination-has-same-colour-piece"));
    return;
  }

  char promotion = 0;
  bool lastRank = (movingPiece == 'P' && destinationRank == 7) ||
                  (movingPiece == 'p' && destinationRank == 0);
  if (lastRank != (length == 5)) {
    Serial.println(F("error:promotion-suffix-required-only-on-final-rank"));
    return;
  }
  if (length == 5) {
    promotion = moveText[4];
    if (promotion >= 'A' && promotion <= 'Z') promotion += 'a' - 'A';
    if ((movingPiece != 'P' && movingPiece != 'p') ||
        (promotion != 'q' && promotion != 'r' && promotion != 'b' && promotion != 'n')) {
      Serial.println(F("error:invalid-promotion"));
      return;
    }
  }

  Serial.print(F("busy:"));
  Serial.println(moveText);

  // En passant: diagonal pawn move to an empty destination.
  bool enPassant = (movingPiece == 'P' || movingPiece == 'p') &&
                   sourceFile != destinationFile && !destinationPiece;
  uint8_t capturedFile = destinationFile;
  uint8_t capturedRank = destinationRank;
  if (enPassant) capturedRank = sourceRank;

  // These special actions need history/physical confirmation. Reject before motion.
  if (enPassant) {
    Serial.println(F("error:en-passant-not-validated"));
    return;
  }
  if (destinationPiece) {
    Serial.println(F("error:capture-drop-outside-h5-not-physically-validated"));
    return;
  }
  if ((movingPiece == 'K' || movingPiece == 'k') &&
      abs((int)destinationFile - (int)sourceFile) == 2) {
    Serial.println(F("error:castling-not-validated"));
    return;
  }

  if (destinationPiece || enPassant) {
    char capturedPiece = board[capturedFile][capturedRank];
    if (enPassant &&
        !((movingPiece == 'P' && capturedPiece == 'p') ||
          (movingPiece == 'p' && capturedPiece == 'P'))) {
      Serial.println(F("error:invalid-en-passant-board-state"));
      return;
    }
    if (!capturedPiece || !removePieceToGraveyard(capturedFile, capturedRank)) {
      Serial.println(F("error:capture-removal-failed; board-state-not-updated"));
      return;
    }
    board[capturedFile][capturedRank] = 0;
  }

  if (!mechanicallyMovePiece(sourceFile, sourceRank, destinationFile, destinationRank)) {
    Serial.println(F("error:piece-move-failed; physical-state-may-be-uncertain"));
    return;
  }

  board[sourceFile][sourceRank] = 0;
  board[destinationFile][destinationRank] = movingPiece;

  // Castling: move the rook after the king.
  if ((movingPiece == 'K' || movingPiece == 'k') &&
      sourceRank == destinationRank &&
      abs((int)destinationFile - (int)sourceFile) == 2) {
    uint8_t rookSourceFile = destinationFile > sourceFile ? 7 : 0;
    uint8_t rookDestinationFile = destinationFile > sourceFile ? destinationFile - 1 : destinationFile + 1;
    char rook = board[rookSourceFile][sourceRank];
    if (!rook || !mechanicallyMovePiece(rookSourceFile, sourceRank,
                                        rookDestinationFile, sourceRank)) {
      Serial.println(F("error:king-moved-but-castling-rook-failed"));
      return;
    }
    board[rookSourceFile][sourceRank] = 0;
    board[rookDestinationFile][sourceRank] = rook;
  }

  if (promotion) {
    board[destinationFile][destinationRank] = isWhite(movingPiece) ? promotion - ('a' - 'A') : promotion;
    Serial.println(F("warning:promotion-requires-manual-physical-piece-replacement"));
    boardTrusted = false;
  }

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
  Serial.print(F(",magnet_angle="));
  Serial.print(magnetAngle);
  Serial.print(F(",steps_per_mm="));
  Serial.println(MOTOR_STEPS_PER_MM, 1);
  Serial.print(F("info:max_step_rate="));
  Serial.print(MAX_STEP_RATE, 1);
  Serial.print(F(",step_acceleration="));
  Serial.println(STEP_ACCELERATION, 1);
  Serial.print(F("info:board_confirmed="));
  Serial.println(boardTrusted ? 1 : 0);
  Serial.print(F("info:square_mm="));
  Serial.print(SQUARE_SIZE_MM, 2);
  Serial.print(F(",invert_a="));
  Serial.print(INVERT_A_DIR ? 1 : 0);
  Serial.print(F(",invert_b="));
  Serial.print(INVERT_B_DIR ? 1 : 0);
  Serial.print(F(",swap_xy="));
  Serial.println(SWAP_X_Y ? 1 : 0);
}

void processCommand(char *command) {
  for (char *p = command; *p; p++) {
    if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
  }

  if (!strcmp(command, "home")) {
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
  } else if (!strcmp(command, "magnet0")) {
    releaseMagnet();
    Serial.println(F("ok:magnet-released-at-0deg"));
  } else if (!strcmp(command, "magnet90")) {
    engageMagnet();
    Serial.println(F("ok:magnet-engaged-at-90deg"));
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
    if (magnetAngle != MAGNET_RELEASE_ANGLE) boardTrusted = false;
    releaseMagnet();
    Serial.println(F("ok:motors-disabled; position-lost; run HOME before moves"));
  } else {
    executeChessMove(command);
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
  magnetServo.write(MAGNET_RELEASE_ANGLE);
  magnetServo.attach(SERVO_PIN);
  releaseMagnet();
  resetBoardState();

  Serial.println(F("OpenMove chess motion controller 0.6"));
  Serial.println(F("ready:place carriage at a1 centre, then send HOME"));
}

void loop() {
  while (Serial.available()) {
    char incoming = Serial.read();
    if (incoming == '\r' || incoming == '\n') {
      if (discardLine) { discardLine = false; inputLength = 0; continue; }
      if (inputLength) {
        inputLine[inputLength] = 0;
        motionAborted = false;
        processCommand(inputLine);
        if (motionAborted) Serial.println(F("error:emergency-stop; position-and-board-uncertain"));
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
