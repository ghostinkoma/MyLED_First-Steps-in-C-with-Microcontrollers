# MyLED

An educational library for Arduino and ESP32 designed to control multiple LEDs simultaneously using **timer interrupts**.
You can blink and dim multiple LEDs with different patterns at the same time without blocking the `loop()` function.

To help beginners learn programming (C++, classes, and microcontrollers), every line of source code includes detailed Japanese comments.

---

## Features

- **Intuitive Syntax like `LED[0].toggle(500)`** — Array-like access using operator overloading.
- **Non-blocking** — Blinking is handled by a 1ms timer interrupt, keeping `loop()` entirely free for other tasks.
- **Multi-platform** — A single sketch works seamlessly across both AVR and ESP32 series.
- **Educational Value** — Learn C++ class design, timer interrupts, and (as an advanced topic) AVR inline assembly.

## Supported Microcontrollers

| Microcontroller | Blink Timer | Control Method (ON/OFF) |
|---|---|---|
| Arduino UNO / Nano (ATmega328P, 16MHz) | Timer1 (CTC, 1kHz) | Inline Assembly (Fast Port I/O) |
| LGT8F328P | Timer1 | Inline Assembly |
| ESP32 / ESP32-C3 / ESP32-S2 / ESP32-S3 | Hardware Timer | `digitalWrite` |
| Others | (None / Blinking Disabled) | `digitalWrite` |

> Differences between microcontrollers are automatically handled internally using `#if defined(...)`.
> **Your sketch code remains exactly the same regardless of the board used.**

## File Structure

```
MyLED/
├── MyLED.ino        … Example sketch (main usage sample)
├── README.md        … This file
├── Reference.md     … Detailed explanation of all functions
├── Class_Design.md  … Internal structure and design details
└── src/
    ├── MyLED.h      … Class declarations (interface)
    └── MyLED.cpp    … Class implementations (logic)
```

## Wiring

```
  MCU Pin ──[Resistor 220–330Ω]──▷|── GND
                                 LED
```

Connect the longer leg of the LED (Anode, +) toward the resistor, and the shorter leg (Cathode, −) to GND.

## Usage (Quick Example)

```cpp
#include "src/MyLED.h"

LEDClass leds[3];          // Declare 3 LED instances

void setup() {
    leds[0].begin(2);      // Register pin assignments
    leds[1].begin(4);
    leds[2].begin(11);

    LED.begin(leds, 3);    // Pass array and count to manager (starts the timer)

    LED[0].toggle(500);    // Blink every 500ms
    LED[1].toggle(200);    // Blink every 200ms
    LED[2].pwm(128);       // Set brightness to 50%
}

void loop() {
    // No code needed here (blinking is handled automatically by interrupts)
}
```

## API Quick Reference

| Function | Description |
|---|---|
| `leds[i].begin(pin)` | Registers the pin for the $i$-th LED and configures pin mode. |
| `LED.begin(leds, 3)` | Manages the array of 3 LEDs together and starts the timer. |
| `LED[i].toggle(t)` | Blinks the LED every $t$ milliseconds (`t = 0` stops blinking). |
| `LED[i].pwm(duty)` | Sets the brightness from 0 to 255 (PWM). |
| `LED[i].setHighInline()` | Immediately turns the LED ON. |
| `LED[i].setLowInline()` | Immediately turns the LED OFF. |

For detailed information, please refer to [Reference.md](Reference.md).

## Notes & Considerations

- **PWM Pin Selection (AVR only)**: This library uses `Timer1` for blinking routines.
  Therefore, **`pwm()` will not work properly on Pins 9 and 10**, which rely on `Timer1`.
  For AVR, select **Pins 3, 5, 6, or 11** when using `pwm()`. For ESP32 series, any general output pin can be used.
- **The parameter `t` in `toggle(t)` represents the "toggle interval"**. For instance, `toggle(500)` keeps the LED ON for 500ms and OFF for 500ms (resulting in a total period of 1 second).
- Because LEDs are accessed within timer interrupts, `toggle()` briefly disables interrupts internally to update settings safely.

## License / Intended Use

Created for educational and experimental purposes. Feel free to modify and adapt the code as needed.