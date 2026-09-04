# Week 2

**Goal this week:** Prepare a safe bench test for the XY motors and pickup servo.

## What we did

- Added an experimental Serial-controlled Arduino hardware smoke-test sketch for
  the CNC Shield X/Y driver channels and the servo.
- Compiled the sketch for Arduino Uno and uploaded it successfully through the
  CP210x adapter at `/dev/ttyUSB0`. The sketch keeps the stepper drivers disabled
  at startup and tests each device only when commanded. No physical motion test
  has been performed yet.
- Corrected the servo signal assignment from A3 to the confirmed D11 wiring and
  uploaded the corrected revision to the Arduino. No motion command was sent;
  the Y motor and driver overheating fault remains unresolved.
- Corrected the connection label: the servo is physically connected to the CNC
  Shield V3 Z+ header, which supplies the configured D11 signal. The user reports
  that only one of the two steppers hums during testing; fault isolation is still
  required.
- The user corrected which sketch worked: it pulses both motors simultaneously
  at approximately 50 steps per second, reverses both together, and moves the
  servo between 30 and 120 degrees. Stored the exact user-supplied automatic
  sketch in `code/hardware_smoke_test/`, compiled it for Arduino Uno, and uploaded
  it via `/dev/ttyUSB0`.
- The user physically ran the uploaded coordinated test and reported that both
  motor motion and servo operation work correctly in this test pattern.

## Problems and blockers

- Driver current limits, individual-motor behavior, and motion under a
  representative mechanical load remain unverified. The previously reported
  X/Y driver temperature difference has not yet been measured or resolved.

### DRV8825 configuration research

#### Abstract

The prototype combines an Arduino Uno-compatible controller, CNC Shield V3, two
DRV8825 carriers, two nominal 1.2 A/phase NEMA 17 motors, and an MG90S servo. The
coordinated 50-step-per-second test works, but one driver was previously reported
hotter and both VREF settings are reported as approximately 0.65 V. Manufacturer
documentation shows that 0.65 V cannot be judged safe until the current-sense
resistor on each actual carrier is identified.

#### Configuration and pulse timing

| Function | Uno pin | Prototype use |
| --- | ---: | --- |
| X STEP / DIR | D2 / D5 | X driver socket |
| Y STEP / DIR | D3 / D6 | Y driver socket |
| Driver ENABLE | D8 | Active-low, shared by both sockets |
| Servo signal | D11 | Shield Z+ header |

The DRV8825 advances on a STEP rising edge and requires STEP high and low times
of at least 1.9 µs. The uploaded sketch uses 10 µs high and 19,990 µs low, so it
meets the electrical timing requirement and produces approximately 50 steps per
second. ENABLE remains low, so both motors stay energized between moves; holding
torque and some heat while stationary are therefore expected.
[TI DRV8825 datasheet](https://www.ti.com/lit/ds/symlink/drv8825.pdf)

#### Interpreting 0.65 V VREF

Texas Instruments specifies:

```text
I_CHOP = VREF / (5 × R_SENSE)
```

At 0.65 V, the calculated full-scale current limit depends on the carrier:

| Resistor marking | Sense resistance | Current limit |
| --- | ---: | ---: |
| R050 | 0.050 Ω | 2.60 A/phase |
| R068 | 0.068 Ω | 1.91 A/phase |
| R100 | 0.100 Ω | 1.30 A/phase |
| R200 | 0.200 Ω | 0.65 A/phase |

The genuine Pololu carrier uses 0.100 Ω resistors, reducing the relationship to
`current limit = VREF × 2`; 0.65 V therefore means 1.30 A. That is about 8% above
a 1.2 A/phase motor rating. Clone carriers can use other resistor values, so the
Pololu multiplier must not be copied until both boards are inspected.
[Pololu DRV8825 carrier guide](https://www.pololu.com/product/2133/)

If both carriers are confirmed R100, 1.2 A corresponds to 0.60 V and a
conservative 0.8 A diagnostic setting corresponds to 0.40 V. These are
calculated values, not permission to adjust unidentified hardware. Confirm the
motor label and resistor markings first.

#### Heat and power

TI shows that conduction loss grows approximately with the square of winding RMS
current and that MOSFET resistance rises with temperature. A modest current-limit
mismatch can therefore produce a noticeable temperature difference. Thermal
shutdown is fault protection, not a target operating mode. Pololu reports that
its carrier generally needs additional cooling above roughly 1.5 A/phase, with
the real limit dependent on airflow, ambient temperature, PCB construction, and
heatsinking.
[TI thermal guidance](https://www.ti.com/lit/ds/symlink/drv8825.pdf),
[Pololu power guidance](https://www.pololu.com/product/2133/)

Pololu also warns that wiring inductance can produce destructive VMOT spikes
even from a 12 V supply and recommends at least 47 µF electrolytic capacitance
close to VMOT and ground on its carrier. Never connect or disconnect a motor
while powered. The MG90S must use a suitable regulated 5 V source—not the 12 V
rail—with its ground connected to Arduino/shield ground.
[Pololu transient and wiring guidance](https://www.pololu.com/product/2133/),
[Arduino servo troubleshooting](https://support.arduino.cc/hc/en-us/articles/360017053760-Troubleshoot-servo-motors),
[Arduino power guidance](https://support.arduino.cc/hc/en-us/articles/360018922259-What-power-supply-can-I-use-with-my-Arduino-board).

#### Validation procedure

1. Disconnect USB and 12 V before touching motors, drivers, jumpers, or wiring.
2. Photograph and record both sense-resistor markings and the exact motor label.
3. Confirm each motor's two coil pairs while the motors are disconnected.
4. Inspect driver orientation, heatsinks, microstep jumpers, connectors, and the
   local VMOT bulk capacitor.
5. Measure X and Y VREF from the same ground reference. Calculate each carrier's
   limit separately with the TI equation.
6. Run the known-working coordinated test briefly and record ambient, X-driver,
   and Y-driver temperatures at equal intervals using the same instrument and
   measurement point.
7. Stop for rapid heating, thermal cycling, resets, smell, smoke, or lost motion.
   Do not increase VREF to cure humming.
8. Test acceleration, higher speed, individual-motor/H-Bot motion, and mechanical
   load one variable at a time only after current and temperature are acceptable.

#### Conclusion

The uploaded firmware has valid STEP timing and coordinated movement works. It
does not validate current limiting. A 0.65 V setting plausibly means 1.30 A/phase
only on an R100 carrier. Until both resistor markings and the exact motor rating
are confirmed, declaring 0.65 V safe would be guesswork. Unequal heating makes
those measurements the next required experiment.

## Decisions

- Experimental: use the standard CNC Shield V3 X/Y pin mapping and the shield's
  D11 Z+ header as the servo signal for the first bench test.

## Next week

- Measure and record both DRV8825 VREF values and compare driver temperatures
  during the validated coordinated test before increasing speed or adding load.

## Links

- Code: [`code/hardware_smoke_test/`](../code/hardware_smoke_test/)
- Photos / CAD:
