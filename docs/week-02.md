# Week 2 — Getting the Motion Electronics to Agree

**Goal this week:** Verify that the Arduino, CNC Shield, motors, drivers, and servo could run together before attaching them to an actual mechanism.

The first week established the hardware direction. This week was about making it move.

The setup was an Arduino Uno with a CNC Shield V3, two DRV8825 stepper drivers, two NEMA 17 motors, and a 12 V, 3 A adapter for the motor supply. A servo would eventually control the magnetic pickup mechanism, so it joined the test too.

At this point, there was no chessboard mechanism attached. No belts, rods, carriage, or pieces. That was intentional. If the electronics could not behave on the desk, giving them an XY mechanism would only make the failure more expensive and harder to understand.

## A small test with a useful job

I uploaded a basic Arduino sketch that pulsed both stepper drivers together, reversed them, and moved the servo between two positions.

The motors ran at around 50 steps per second. Slow enough to watch, slow enough to stop, and fast enough to prove that the Arduino, CNC Shield, and drivers were actually talking to each other.

Both motors moved together and reversed together. The servo also responded to its test positions.

That was the first useful result: the control chain worked end to end.

```text
Arduino Uno → CNC Shield → DRV8825 drivers → NEMA 17 motors
                                      └────→ servo test
```

It did not prove the final mechanism would work. It only proved that the electronics could produce controlled movement before any real mechanical load entered the conversation.

## The VREF problem was not solved by finding a number

Both DRV8825 drivers measured approximately **0.65 V VREF**.

That was useful, but not a final answer. VREF controls the driver’s current limit, but the actual limit also depends on the current-sense resistors on the board. Two drivers that look identical can use different resistor values, especially when they are clone boards.

So `0.65 V` was a measurement, not a safety certificate.

The sensible next step was to identify the resistor markings, calculate the current limit properly, and compare the driver temperatures under the same test conditions. Turning a tiny adjustment screw until a motor sounds more confident is not calibration. It is just a very compact way to create new problems.

## What did not behave perfectly

Earlier testing had produced a motor hum, and the Y-axis driver appeared hotter than the X-axis driver.

The coordinated low-speed test worked, but it did not explain that difference. It could have been the motor wiring, driver-current configuration, the specific driver board, or something else in the setup. None of those possibilities should be promoted to a conclusion without testing them separately.

The motors also had not been tested with belts, rods, carriage friction, or the weight of a pickup system. A motor moving freely on a desk has not yet earned the right to be called an XY mechanism.

## What I learned

* The Uno, CNC Shield, two drivers, motors, and servo can run in one controlled bench test.
* The 12 V, 3 A adapter powered the stepper side for that test.
* The two VREF readings were approximately 0.65 V.
* A successful motion test does not validate current limiting, driver temperature, or mechanical reliability.
* The physical board still needed to exist.

## Next week

* Inspect both DRV8825 boards and identify their sense-resistor values.
* Compare driver temperatures during repeatable tests.
* Test each motor independently.
* Start building the first XY-motion prototype and find out which assumptions survive contact with hardware.

## Links

* Code: `code/hardware_smoke_test/`
* Photos / CAD:
