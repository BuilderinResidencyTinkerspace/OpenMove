#include <Servo.h>
#include <math.h>
#include <string.h>

// Coordinate contract shared with every host controller. Do not change one
// field independently: a1 is the only manual home, white starts on ranks 1-2,
// +X advances files a->h, and +Y advances ranks 1->8.
const uint8_t PROTOCOL_VERSION = 6;
const char FIRMWARE_VERSION[] = "2.5";

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
const float SQUARE_SIZE_MM = 44.0f; // User-measured adjacent square centres.
const float PLAYABLE_SIZE_MM = SQUARE_SIZE_MM * 8.0f;
const float HALF_SQUARE_MM = SQUARE_SIZE_MM / 2.0f;
// a1 is (0, 0), h8 is (308, 308), and the positive playable edges are at
// (330, 330). Remaining travel is reserved for edge operations.
const float MAX_X_MM = PLAYABLE_SIZE_MM;
const float MAX_Y_MM = PLAYABLE_SIZE_MM;
// Change only after measuring the physical jog directions.
const bool INVERT_A_DIR = true;
const bool INVERT_B_DIR = true;
const bool SWAP_X_Y = false;
// Physical test: the previous logical X+ moved b1->a1. Reverse X so logical
// X+ moves a1->b1. Y is not reversed: Y+ already moved a1->a2.
const bool REVERSE_LOGICAL_X = true;

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

struct RelocationPlan {
  char piece;
  uint8_t source;
  uint8_t parking;
  uint8_t pathLength;
  uint8_t path[64];
};

struct KnightPlan {
  bool valid;
  uint8_t route[3];
  uint8_t relocationCount;
  uint16_t loadedSteps;
  RelocationPlan relocations[2];
};

KnightPlan knightPlan;
int8_t plannerParent[64];
uint8_t plannerQueue[64];

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
  float logicalX = REVERSE_LOGICAL_X ? -x : x;
  float mappedX = SWAP_X_Y ? y : logicalX;
  float mappedY = SWAP_X_Y ? logicalX : y;
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

uint8_t squareIndex(uint8_t file, uint8_t rank) { return rank * 8 + file; }
uint8_t indexFile(uint8_t index) { return index % 8; }
uint8_t indexRank(uint8_t index) { return index / 8; }

bool routeContains(const uint8_t *route, uint8_t square) {
  return route[0] == square || route[1] == square || route[2] == square;
}

bool findEmptyPath(char state[8][8], uint8_t source, uint8_t destination,
                   const uint8_t *forbiddenRoute, uint8_t *path, uint8_t &pathLength) {
  memset(plannerParent, -1, sizeof(plannerParent));
  uint8_t head = 0;
  uint8_t tail = 0;
  plannerQueue[tail++] = source;
  plannerParent[source] = source;

  const int8_t fileSteps[4] = {1, -1, 0, 0};
  const int8_t rankSteps[4] = {0, 0, 1, -1};
  while (head < tail && plannerParent[destination] < 0) {
    uint8_t current = plannerQueue[head++];
    int currentFile = indexFile(current);
    int currentRank = indexRank(current);
    for (uint8_t direction = 0; direction < 4; direction++) {
      int nextFile = currentFile + fileSteps[direction];
      int nextRank = currentRank + rankSteps[direction];
      if (nextFile < 0 || nextFile > 7 || nextRank < 0 || nextRank > 7) continue;
      uint8_t next = squareIndex(nextFile, nextRank);
      if (plannerParent[next] >= 0 ||
          (next != destination && state[nextFile][nextRank]) ||
          (next != source && next == forbiddenRoute[2])) continue;
      plannerParent[next] = current;
      plannerQueue[tail++] = next;
    }
  }
  if (plannerParent[destination] < 0) return false;

  pathLength = 0;
  uint8_t current = destination;
  while (current != source) {
    path[pathLength++] = current;
    current = plannerParent[current];
  }
  path[pathLength++] = source;
  for (uint8_t left = 0, right = pathLength - 1; left < right; left++, right--) {
    uint8_t temporary = path[left];
    path[left] = path[right];
    path[right] = temporary;
  }
  return true;
}

void buildKnightRoute(uint8_t sourceFile, uint8_t sourceRank,
                      uint8_t destinationFile, uint8_t destinationRank,
                      uint8_t sequence, uint8_t *route) {
  int8_t fileStep = destinationFile > sourceFile ? 1 : -1;
  int8_t rankStep = destinationRank > sourceRank ? 1 : -1;
  uint8_t file = sourceFile;
  uint8_t rank = sourceRank;
  bool longFileAxis = abs((int)destinationFile - sourceFile) == 2;

  for (uint8_t step = 0; step < 3; step++) {
    bool moveLongAxis = sequence == 0 ? step < 2 : sequence == 1 ? step != 1 : step > 0;
    if (longFileAxis == moveLongAxis) file += fileStep;
    else rank += rankStep;
    route[step] = squareIndex(file, rank);
  }
}

bool candidateParkingPath(char state[8][8], uint8_t blocker, const uint8_t *route,
                          uint8_t skipCandidates, uint8_t *parking,
                          uint8_t *path, uint8_t &pathLength) {
  uint8_t blockerFile = indexFile(blocker);
  uint8_t blockerRank = indexRank(blocker);
  for (uint8_t distance = 1; distance <= 14; distance++) {
    for (uint8_t rank = 0; rank < 8; rank++) {
      for (uint8_t file = 0; file < 8; file++) {
        if (emergencyRequested()) {
          stopController();
          return false;
        }
        uint8_t candidate = squareIndex(file, rank);
        if (state[file][rank] || routeContains(route, candidate) ||
            abs((int)file - blockerFile) + abs((int)rank - blockerRank) != distance) continue;
        if (findEmptyPath(state, blocker, candidate, route, path, pathLength)) {
          if (skipCandidates) {
            skipCandidates--;
            continue;
          }
          *parking = candidate;
          return true;
        }
      }
    }
  }
  return false;
}

uint8_t gridDistance(uint8_t first, uint8_t second) {
  return abs((int)indexFile(first) - indexFile(second)) +
         abs((int)indexRank(first) - indexRank(second));
}

uint16_t carriageDistanceTo(uint8_t square) {
  float distance = fabs(carriageX - squareX(indexFile(square))) +
                   fabs(carriageY - squareY(indexRank(square)));
  return lroundf(distance / SQUARE_SIZE_MM);
}

bool buildRelocationPlan(const uint8_t *route, const uint8_t *blockers,
                         uint8_t blockerCount, bool reverseOrder, uint8_t knightSource,
                         KnightPlan &candidate) {
  candidate.relocationCount = blockerCount;
  candidate.loadedSteps = 3 + carriageDistanceTo(knightSource);
  if (!blockerCount) return true;

  uint8_t firstBlockerIndex = reverseOrder ? blockerCount - 1 : 0;
  uint8_t firstBlocker = blockers[firstBlockerIndex];
  bool found = false;
  uint16_t bestLoadedSteps = 0;
  for (uint8_t skippedFirst = 0; skippedFirst < 64; skippedFirst++) {
    char state[8][8];
    memcpy(state, board, sizeof(state));
    state[indexFile(route[2])][indexRank(route[2])] = 0;

    RelocationPlan first;
    first.piece = state[indexFile(firstBlocker)][indexRank(firstBlocker)];
    first.source = firstBlocker;
    if (!candidateParkingPath(state, firstBlocker, route, skippedFirst, &first.parking,
                              first.path, first.pathLength)) break;
    state[indexFile(firstBlocker)][indexRank(firstBlocker)] = 0;
    state[indexFile(first.parking)][indexRank(first.parking)] = first.piece;

    RelocationPlan second;
    uint16_t loadedSteps = 3 + 2 * (first.pathLength - 1) +
                 carriageDistanceTo(first.source) +
                 gridDistance(first.parking, knightSource) +
                 gridDistance(route[2], first.parking);
    if (blockerCount == 2) {
      uint8_t secondBlockerIndex = reverseOrder ? 0 : 1;
      uint8_t secondBlocker = blockers[secondBlockerIndex];
      second.piece = state[indexFile(secondBlocker)][indexRank(secondBlocker)];
      second.source = secondBlocker;
      if (!candidateParkingPath(state, secondBlocker, route, 0, &second.parking,
                                second.path, second.pathLength)) continue;
      loadedSteps += 2 * (second.pathLength - 1);
      loadedSteps -= gridDistance(first.parking, knightSource) +
             gridDistance(route[2], first.parking);
      loadedSteps += gridDistance(first.parking, second.source) +
             gridDistance(second.parking, knightSource) +
             gridDistance(route[2], second.parking) +
             gridDistance(second.source, first.parking);
    }

    if (!found || loadedSteps < bestLoadedSteps) {
      found = true;
      bestLoadedSteps = loadedSteps;
      candidate.relocations[0] = first;
      if (blockerCount == 2) candidate.relocations[1] = second;
    }
  }
  candidate.loadedSteps = bestLoadedSteps;
  return found;
}

bool planKnightMove(uint8_t sourceFile, uint8_t sourceRank,
                    uint8_t destinationFile, uint8_t destinationRank) {
  knightPlan.valid = false;
  uint8_t knightSource = squareIndex(sourceFile, sourceRank);
  for (uint8_t sequence = 0; sequence < 3; sequence++) {
    uint8_t route[3];
    buildKnightRoute(sourceFile, sourceRank, destinationFile, destinationRank, sequence, route);
    uint8_t blockers[2];
    uint8_t blockerCount = 0;
    for (uint8_t step = 0; step < 2; step++) {
      if (board[indexFile(route[step])][indexRank(route[step])]) blockers[blockerCount++] = route[step];
    }

    uint8_t orderCount = blockerCount == 2 ? 2 : 1;
    for (uint8_t order = 0; order < orderCount; order++) {
      KnightPlan candidate;
      candidate.valid = true;
      memcpy(candidate.route, route, sizeof(route));
      if (!buildRelocationPlan(route, blockers, blockerCount, order == 1,
               knightSource, candidate)) continue;
      if (!knightPlan.valid || candidate.relocationCount < knightPlan.relocationCount ||
          (candidate.relocationCount == knightPlan.relocationCount &&
           candidate.loadedSteps < knightPlan.loadedSteps)) {
        knightPlan = candidate;
      }
    }
  }
  return knightPlan.valid;
}

bool carryAlongPath(const uint8_t *path, uint8_t pathLength, bool reverse) {
  uint8_t source = reverse ? path[pathLength - 1] : path[0];
  if (!pickupSquare(indexFile(source), indexRank(source))) return false;
  for (uint8_t step = 1; step < pathLength; step++) {
    uint8_t index = reverse ? path[pathLength - 1 - step] : path[step];
    if (!moveTo(squareX(indexFile(index)), squareY(indexRank(index)))) {
      releaseMagnet();
      return false;
    }
  }
  releaseMagnet();
  return !motionAborted;
}

bool executeKnightPlan(uint8_t sourceFile, uint8_t sourceRank,
                       uint8_t destinationFile, uint8_t destinationRank) {
  char knight = board[sourceFile][sourceRank];
  for (uint8_t relocation = 0; relocation < knightPlan.relocationCount; relocation++) {
    RelocationPlan &planned = knightPlan.relocations[relocation];
    if (!carryAlongPath(planned.path, planned.pathLength, false)) return false;
    board[indexFile(planned.source)][indexRank(planned.source)] = 0;
    board[indexFile(planned.parking)][indexRank(planned.parking)] = planned.piece;
  }

  if (!pickupSquare(sourceFile, sourceRank)) return false;
  for (uint8_t step = 0; step < 3; step++) {
    uint8_t target = knightPlan.route[step];
    if (!moveTo(squareX(indexFile(target)), squareY(indexRank(target)))) {
      releaseMagnet();
      return false;
    }
  }
  releaseMagnet();
  if (motionAborted) return false;
  board[sourceFile][sourceRank] = 0;
  board[destinationFile][destinationRank] = knight;

  for (int8_t relocation = knightPlan.relocationCount - 1; relocation >= 0; relocation--) {
    RelocationPlan &planned = knightPlan.relocations[relocation];
    if (!carryAlongPath(planned.path, planned.pathLength, true)) return false;
    board[indexFile(planned.parking)][indexRank(planned.parking)] = 0;
    board[indexFile(planned.source)][indexRank(planned.source)] = planned.piece;
  }
  return true;
}

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

bool mechanicallyMovePiece(uint8_t sourceFile, uint8_t sourceRank,
                           uint8_t destinationFile, uint8_t destinationRank) {
  if (board[sourceFile][sourceRank] == 'N') {
    return executeKnightPlan(sourceFile, sourceRank, destinationFile, destinationRank);
  }
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

  char boardPiece = requestedType;
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
                planKnightMove(1, 0, 2, 2) &&
                knightPlan.relocationCount == 1;

  memcpy(board, savedBoard, sizeof(board));
  whiteToMove = savedWhiteToMove;
  Serial.println(passed ? F("ok:selftest-passed; no-motion") : F("error:selftest-failed; no-motion"));
}

void rejectChessMove(const __FlashStringHelper *message) {
  Serial.println(message);
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
  if (isBlack(movingPiece)) {
    rejectChessMove(F("error:black-moves-disabled; only-white-is-automated"));
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
  if (destinationPiece && isWhite(movingPiece)) {
    rejectChessMove(F("error:white-captures-disabled; remove-piece-manually"));
    return;
  }
  if (movingPiece == 'N' &&
      !planKnightMove(sourceFile, sourceRank, destinationFile, destinationRank)) {
    rejectChessMove(F("error:no-reversible-knight-relocation-plan"));
    return;
  }

  Serial.print(F("busy:"));
  Serial.println(moveText);

  if (!mechanicallyMovePiece(sourceFile, sourceRank, destinationFile, destinationRank)) {
    boardTrusted = false;
    if (motionAborted) return;
    Serial.println(F("error:piece-move-failed; physical-state-may-be-uncertain"));
    return;
  }

  board[sourceFile][sourceRank] = 0;
  board[destinationFile][destinationRank] = movingPiece;
  whiteToMove = true;

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
  Serial.print(F("info:protocol="));
  Serial.print(PROTOCOL_VERSION);
  Serial.println(F(",home_square=a1,x_axis=a-to-h,y_axis=1-to-8,white_ranks=1-2,x_motor_direction=reverse"));
  Serial.println(F("info:side_to_move=white"));
  Serial.println(F("info:motion_side=white; black_moves_disabled"));
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
  Serial.print(SWAP_X_Y ? 1 : 0);
  Serial.print(F(",reverse_logical_x="));
  Serial.println(REVERSE_LOGICAL_X ? 1 : 0);
  Serial.print(F("info:emergency_stop_latched="));
  Serial.println(emergencyStopLatched ? 1 : 0);
  Serial.println(F("info:knight_planner=temporary-blocker-relocation"));
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

  Serial.print(F("OpenMove chess motion controller "));
  Serial.println(FIRMWARE_VERSION);
  Serial.println(F("mapping:home=a1,+x=a-to-h,+y=1-to-8,white=ranks-1-2"));
  Serial.println(F("mode:white-only automation; black moves disabled"));
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
