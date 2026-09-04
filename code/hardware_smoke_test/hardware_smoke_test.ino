#include <Servo.h>

const byte X_STEP_PIN = 2;
const byte Y_STEP_PIN = 3;
const byte X_DIR_PIN = 5;
const byte Y_DIR_PIN = 6;
const byte ENABLE_PIN = 8;
const byte SERVO_PIN = 11;

Servo testServo;

const int STEPS_PER_MOVE = 100;
const unsigned int STEP_LOW_TIME_US = 19990;  // 20 ms period: 50 steps/second

void stepMotorsTogether(int numberOfSteps) {
  for (int step = 0; step < numberOfSteps; step++) {
    digitalWrite(X_STEP_PIN, HIGH);
    digitalWrite(Y_STEP_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(X_STEP_PIN, LOW);
    digitalWrite(Y_STEP_PIN, LOW);
    delayMicroseconds(STEP_LOW_TIME_US);
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
