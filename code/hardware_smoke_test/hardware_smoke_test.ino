#include <Servo.h>
#include <math.h>

const byte X_STEP_PIN = 2;
const byte Y_STEP_PIN = 3;
const byte X_DIR_PIN = 5;
const byte Y_DIR_PIN = 6;
const byte ENABLE_PIN = 8;
const byte SERVO_PIN = 11;

Servo testServo;

const int STEPS_PER_MOVE = 100;
const float START_RATE_SPS = 50.0;
const float MAX_RATE_SPS = 800.0;       // 10 mm/s if the drivers use 1/16 step.
const float ACCELERATION_SPS2 = 4000.0; // 50 mm/s^2 at an assumed 80 steps/mm.
const unsigned int STEP_HIGH_TIME_US = 10;

void waitMicroseconds(unsigned long durationUs) {
  if (durationUs >= 1000UL) {
    delay(durationUs / 1000UL);
    durationUs %= 1000UL;
  }
  if (durationUs > 0) {
    delayMicroseconds(static_cast<unsigned int>(durationUs));
  }
}

void stepMotorsTogether(int numberOfSteps) {
  for (int step = 0; step < numberOfSteps; step++) {
    const int stepsFromNearestEnd = min(step, numberOfSteps - 1 - step);
    const float rate = min(
        MAX_RATE_SPS,
        sqrt(START_RATE_SPS * START_RATE_SPS +
             2.0 * ACCELERATION_SPS2 * stepsFromNearestEnd));
    const unsigned long stepPeriodUs =
        static_cast<unsigned long>(1000000.0 / rate);

    digitalWrite(X_STEP_PIN, HIGH);
    digitalWrite(Y_STEP_PIN, HIGH);
    delayMicroseconds(STEP_HIGH_TIME_US);
    digitalWrite(X_STEP_PIN, LOW);
    digitalWrite(Y_STEP_PIN, LOW);
    waitMicroseconds(stepPeriodUs - STEP_HIGH_TIME_US);
  }
}

void setup() {
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(X_STEP_PIN, OUTPUT);
  pinMode(Y_STEP_PIN, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);

  digitalWrite(X_STEP_PIN, LOW);
  digitalWrite(Y_STEP_PIN, LOW);
  digitalWrite(X_DIR_PIN, LOW);
  digitalWrite(Y_DIR_PIN, LOW);
  digitalWrite(ENABLE_PIN, LOW);  // CNC Shield V3/DRV8825 enable is active LOW

  testServo.attach(SERVO_PIN);
  testServo.write(30);
  delay(1000);
}

void loop() {
  testServo.write(30);
  digitalWrite(X_DIR_PIN, LOW);
  digitalWrite(Y_DIR_PIN, LOW);
  delay(10);  // Direction setup time before stepping
  stepMotorsTogether(STEPS_PER_MOVE);

  delay(2000);

  testServo.write(120);
  digitalWrite(X_DIR_PIN, HIGH);
  digitalWrite(Y_DIR_PIN, HIGH);
  delay(10);
  stepMotorsTogether(STEPS_PER_MOVE);

  delay(2000);
}
