# Week 1 — First Smoke 

**Goal this week:** Get the first motion electronics powered safely enough to experiment with.

## What I did

This week was less “automatic chessboard” and more “can I make these motors do something without turning a driver into a small space heater?”

I started experimenting with the Arduino Uno, CNC Shield V3, stepper drivers, and the stepper-motor power supply.


<img width="870" height="447" alt="image" src="https://github.com/user-attachments/assets/e9a06f79-cc27-4874-8202-eb1a5f34cbc3" />
<img width="733" height="327" alt="image" src="https://github.com/user-attachments/assets/e5aebdc2-d27c-4e62-8648-96d06f3b1f18" />


The motion setup used:
* Arduino Uno
* CNC Shield V3
* Two DRV8825 stepper drivers
* Two NEMA 17 stepper motors
* A 12 V, 3 A adapter for motor power

The CNC Shield gave the Arduino a more practical way to control the stepper drivers, but it also added a few things that needed to be understood before sending motion commands: driver orientation, motor wiring, shared enable behaviour, and especially the DRV8825 current limit.

## Figuring out VREF

The DRV8825 drivers use a small VREF adjustment to set their current limit. It is very tempting to treat that tiny potentiometer like a volume knob and turn it until the motor sounds confident. That would be a bad plan.

I checked both drivers and found their VREF values were approximately:

| Driver        | Measured VREF |
| ------------- | ------------: |
| X-axis driver |        0.65 V |
| Y-axis driver |        0.65 V |

That did not automatically prove the current limit was correct. The actual current depends on the sense resistors fitted to each driver board, and clone DRV8825 boards are not guaranteed to use the same resistor values.

So the important result from this week was not “the drivers are perfectly configured.” It was that I had a measured starting point instead of guessing.

## What I learned

* The Arduino, CNC Shield, stepper drivers, motors, and power supply need to be treated as one system.
* A 12 V adapter can still cause trouble if the driver current limit or wiring is wrong.
* VREF is measured in volts, but it controls motor current indirectly.
* “The motor is humming” and “the motor is correctly configured” are very different statements.

## Problems and blockers

* The actual current limit could not be confirmed until the sense-resistor markings on both DRV8825 boards were identified.
* Stepper-driver temperature behaviour still needed to be checked during longer tests.
* The motors had not yet been validated under the real XY mechanism or a representative load.
* The chessboard itself was still an idea at this stage; this work only established the first electronics test setup.

## Decisions

* Use the Arduino Uno and CNC Shield V3 as the first low-level motion-control setup.
* Use the 12 V, 3 A adapter as the bench power source for the stepper side.
* Keep the DRV8825 VREF values at approximately 0.65 V until the driver hardware and current limits are properly verified.
* Treat all motion hardware as experimental until it works reliably under a real mechanical load.
