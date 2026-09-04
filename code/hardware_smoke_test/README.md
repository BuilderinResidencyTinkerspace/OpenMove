# Coordinated hardware test

This experimental Arduino sketch continuously tests coordinated X/Y motor motion
and the servo on an Arduino Uno with CNC Shield V3.

## Connections

| Device | CNC Shield V3 connection | Arduino pin |
| --- | --- | --- |
| X driver STEP / DIR | X socket | D2 / D5 |
| Y driver STEP / DIR | Y socket | D3 / D6 |
| Shared driver ENABLE | Shield enable | D8, active-low |
| Servo signal | Z+ limit-switch header | D11 |

Both motors receive 100 simultaneous step pulses at approximately 50 steps per
second, pause, reverse together for 100 pulses, and repeat. The servo alternates
between 30 and 120 degrees. The sketch begins automatically after reset and
keeps both DRV8825 drivers enabled continuously.

The servo must use a suitable regulated 5 V supply with its ground connected to
Arduino/shield ground. Never connect the servo to the 12 V motor supply. Never
connect or disconnect a motor while power is applied.

This test does not validate individual-motor motion, driver current limits,
temperature, positioning accuracy, homing, limits, or behavior under a
representative mechanical load.
