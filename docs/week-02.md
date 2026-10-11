# Week 2  : Getting the Motion Electronics to Agree

**Goal this week:** Verify that the Arduino, CNC Shield, motors, drivers, and servo could run together before attaching them to an actual mechanism.

The first week established the hardware direction. This week was about making it move.

The setup was an Arduino Uno with a CNC Shield V3, two DRV8825 stepper drivers, two NEMA 17 motors, and a 12 V, 3 A adapter for the motor supply. A servo would eventually control the magnetic pickup mechanism, so it joined the test too.

At this point, there was no chessboard mechanism attached. No belts, rods, carriage, or pieces. That was intentional. If the electronics could not behave on the desk, giving them an XY mechanism would only make the failure more expensive and harder to understand.

<img width="733" height="327" alt="image" src="https://github.com/user-attachments/assets/44b3dea2-9b8f-4f55-b09f-58d114e383ea" />

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

## What the sketch actually asked the hardware to do

The [test sketch](../code/hardware_smoke_test/hardware_smoke_test.ino) sends
100 simultaneous step pulses to the two driver channels, pauses, reverses both,
and repeats. At roughly 50 pulses per second, each run of 100 pulses takes
about two seconds before the pause. The servo alternates between 30 and
120 degrees.

Those are commanded pulses and servo positions. They are not measurements of
carriage travel or magnet height. There was no assembled XY mechanism in this
experiment to measure in the first place.

The confirmed connections are worth keeping close to the result:

| Signal | Uno pin | Shield connection |
| --- | --- | --- |
| Motor A STEP / DIR | D2 / D5 | X driver socket |
| Motor B STEP / DIR | D3 / D6 | Y driver socket |
| Shared enable | D8, active-low | Both driver channels |
| Servo signal | D11 | Z+ header |

The servo receives its signal through that header; its power needs suitable
regulated 5 V and a shared ground. The motor supply is 12 V. Those two power
rails do not become interchangeable because their wires fit nearby.

## The result had a very specific boundary

The uploaded coordinated test was physically run and reported to work. That
supports a small, useful conclusion: this setup could pulse both motors together
and operate the servo at the chosen test settings.

It does not isolate either motor channel, explain the earlier temperature
difference, or validate a belt transform. In the later experimental H-bot
mapping, carriage motion depends on the combination of both motors. Shield
labels alone cannot tell me which direction the carriage will travel.

The next layer was giving the magnet an actual mechanism to live in:
[Week 3](week-03.md). The electronics had earned another experiment, not a
finished-product sticker.
