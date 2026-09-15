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
const float BOARD_SIZE_MM = 300.0f;
const float SQUARE_SIZE_MM = 37.5f;
const float HALF_SQUARE_MM = 18.75f;

// Experimental capture drop: board edge immediately outside h5.
// The user has not yet physically verified that a released piece falls here.
const float GRAVEYARD_X_MM = 300.0f;
const float GRAVEYARD_Y_MM = 168.75f;
const float GRAVEYARD_APPROACH_Y_MM = 187.5f;

const uint8_t MAGNET_RELEASE_ANGLE = 0;
const uint8_t MAGNET_ENGAGE_ANGLE = 90;
const unsigned long SERVO_SETTLE_MS = 700;

// Conservative initial motion values. These require physical validation.
const float START_STEP_RATE = 80.0f;
const float MAX_STEP_RATE = 650.0f;
const float STEP_ACCELERATION = 1000.0f;
const unsigned int STEP_HIGH_US = 10;

Servo magnetServo;

long motorAPosition = 0;
long motorBPosition = 0;
float carriageX = 0.0f;
float carriageY = 0.0f;
bool manuallyHomed = false;
bool motionAborted = false;

char board[8][8];
char inputLine[32];
uint8_t inputLength = 0;

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
  delay(SERVO_SETTLE_MS);
}

void engageMagnet() {
  magnetServo.write(MAGNET_ENGAGE_ANGLE);
  delay(SERVO_SETTLE_MS);
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
  }
  return false;
}

void setDirection(uint8_t pin, long delta) {
  digitalWrite(pin, delta >= 0 ? HIGH : LOW);
}

bool moveMotors(long targetA, long targetB) {
  long deltaA = targetA - motorAPosition;
  long deltaB = targetB - motorBPosition;
  unsigned long stepsA = labs(deltaA);
  unsigned long stepsB = labs(deltaB);
  unsigned long totalEvents = max(stepsA, stepsB);

  if (totalEvents == 0) return true;

  setDirection(A_DIR_PIN, deltaA);
  setDirection(B_DIR_PIN, deltaB);
  delayMicroseconds(20);

  long errorA = -(long)totalEvents / 2;
  long errorB = -(long)totalEvents / 2;
  motionAborted = false;

  for (unsigned long event = 0; event < totalEvents; event++) {
    if (emergencyRequested()) {
      motionAborted = true;
      digitalWrite(ENABLE_PIN, HIGH);
      releaseMagnet();
      manuallyHomed = false;
      Serial.println(F("error:emergency-stop; position-lost; run HOME"));
      return false;
    }

    bool stepA = false;
    bool stepB = false;
    errorA += stepsA;
    errorB += stepsB;
    if (errorA >= 0) {
      errorA -= totalEvents;
      stepA = true;
    }
    if (errorB >= 0) {
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
  if (x < 0.0f || x > BOARD_SIZE_MM || y < 0.0f || y > BOARD_SIZE_MM) {
    Serial.println(F("error:target-outside-300mm-workspace"));
    return false;
  }

  // H-bot/CoreXY transform. On the validated OpenMove belt routing, logical X
  // (a-file toward h-file) requires B = Y-X rather than X-Y.
  long targetA = lroundf((x + y) * MOTOR_STEPS_PER_MM);
  long targetB = lroundf((y - x) * MOTOR_STEPS_PER_MM);
  if (!moveMotors(targetA, targetB)) return false;
  carriageX = x;
  carriageY = y;
  return true;
}

float squareX(uint8_t file) { return HALF_SQUARE_MM + file * SQUARE_SIZE_MM; }
float squareY(uint8_t rank) { return HALF_SQUARE_MM + rank * SQUARE_SIZE_MM; }

float safeHorizontalLane(uint8_t rank) {
  if (rank == 0) return SQUARE_SIZE_MM;
  return rank * SQUARE_SIZE_MM;
}

float safeVerticalLane(uint8_t sourceFile, uint8_t destinationFile) {
  if (sourceFile == 0) return SQUARE_SIZE_MM;
  if (sourceFile == 7) return 7.0f * SQUARE_SIZE_MM;
  if (destinationFile >= sourceFile) return (sourceFile + 1) * SQUARE_SIZE_MM;
  return sourceFile * SQUARE_SIZE_MM;
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
  return true;
}

bool removePieceToGraveyard(uint8_t file, uint8_t rank) {
  if (!pickupSquare(file, rank)) return false;

  // Enter an internal lane, approach the right edge between ranks 5 and 6,
  // then slide along the edge to the requested position outside h5.
  float sourceLaneY = safeHorizontalLane(rank);
  float internalRightLaneX = 7.0f * SQUARE_SIZE_MM;
  if (!moveTo(squareX(file), sourceLaneY) ||
      !moveTo(internalRightLaneX, sourceLaneY) ||
      !moveTo(internalRightLaneX, GRAVEYARD_APPROACH_Y_MM) ||
      !moveTo(GRAVEYARD_X_MM, GRAVEYARD_APPROACH_Y_MM) ||
      !moveTo(GRAVEYARD_X_MM, GRAVEYARD_Y_MM)) return false;

  releaseMagnet();
  Serial.println(F("warning:graveyard-drop-not-sensor-verified"));
  return true;
}

bool mechanicallyMovePiece(uint8_t sourceFile, uint8_t sourceRank,
                           uint8_t destinationFile, uint8_t destinationRank) {
  if (!pickupSquare(sourceFile, sourceRank)) return false;
  if (!carryBetweenSquares(sourceFile, sourceRank, destinationFile, destinationRank)) return false;
  releaseMagnet();
  return true;
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
  if (!manuallyHomed) {
    Serial.println(F("error:not-homed; place carriage at board corner and send HOME"));
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
  }

  Serial.print(F("ok:"));
  Serial.println(moveText);
}

void printStatus() {
  Serial.print(F("status:homed="));
  Serial.print(manuallyHomed ? 1 : 0);
  Serial.print(F(",x="));
  Serial.print(carriageX, 2);
  Serial.print(F(",y="));
  Serial.print(carriageY, 2);
  Serial.print(F(",magnet=0deg-release,steps_per_mm="));
  Serial.println(MOTOR_STEPS_PER_MM, 1);
}

void processCommand(char *command) {
  for (char *p = command; *p; p++) {
    if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
  }

  if (!strcmp(command, "home")) {
    releaseMagnet();
    motorAPosition = 0;
    motorBPosition = 0;
    carriageX = 0.0f;
    carriageY = 0.0f;
    manuallyHomed = true;
    digitalWrite(ENABLE_PIN, LOW);
    Serial.println(F("ok:manual-home-set-at-board-corner; motors-enabled"));
  } else if (!strcmp(command, "status")) {
    printStatus();
  } else if (!strcmp(command, "jogx+10") || !strcmp(command, "jogx-10") ||
             !strcmp(command, "jogy+10") || !strcmp(command, "jogy-10")) {
    if (!manuallyHomed) {
      Serial.println(F("error:not-homed; run HOME first"));
      return;
    }
    releaseMagnet();
    float targetX = carriageX;
    float targetY = carriageY;
    if (!strcmp(command, "jogx+10")) targetX += 10.0f;
    if (!strcmp(command, "jogx-10")) targetX -= 10.0f;
    if (!strcmp(command, "jogy+10")) targetY += 10.0f;
    if (!strcmp(command, "jogy-10")) targetY -= 10.0f;
    if (moveTo(targetX, targetY)) Serial.println(F("ok:jog-complete"));
  } else if (!strcmp(command, "resetboard")) {
    resetBoardState();
    Serial.println(F("ok:internal-board-reset-to-standard-starting-position"));
  } else if (!strcmp(command, "disable")) {
    releaseMagnet();
    digitalWrite(ENABLE_PIN, HIGH);
    manuallyHomed = false;
    Serial.println(F("ok:motors-disabled; position-lost; run HOME before moves"));
  } else {
    executeChessMove(command);
  }
}

void setup() {
  pinMode(A_STEP_PIN, OUTPUT);
  pinMode(B_STEP_PIN, OUTPUT);
  pinMode(A_DIR_PIN, OUTPUT);
  pinMode(B_DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  digitalWrite(A_STEP_PIN, LOW);
  digitalWrite(B_STEP_PIN, LOW);
  digitalWrite(ENABLE_PIN, HIGH); // Safe startup: motors disabled until manual HOME.

  magnetServo.attach(SERVO_PIN);
  releaseMagnet();
  resetBoardState();

  Serial.begin(115200);
  Serial.println(F("OpenMove chess motion controller 0.1"));
  Serial.println(F("ready:place carriage at board corner, then send HOME"));
}

void loop() {
  while (Serial.available()) {
    char incoming = Serial.read();
    if (incoming == '\r' || incoming == '\n') {
      if (inputLength) {
        inputLine[inputLength] = 0;
        processCommand(inputLine);
        inputLength = 0;
      }
    } else if (incoming == '!' || incoming == 0x18) {
      digitalWrite(ENABLE_PIN, HIGH);
      releaseMagnet();
      manuallyHomed = false;
      inputLength = 0;
      Serial.println(F("error:emergency-stop; position-lost; run HOME"));
    } else if (inputLength < sizeof(inputLine) - 1) {
      inputLine[inputLength++] = incoming;
    } else {
      inputLength = 0;
      Serial.println(F("error:command-too-long"));
    }
  }
}
