# MyLED Reference Manual

This document provides detailed specifications for each class and member function. Refer to this document whenever you need detailed usage instructions.

* **Target Header**: [`src/MyLED.h`](src/MyLED.h)
* **Implementation**: [`src/MyLED.cpp`](src/MyLED.cpp)

---

## General Workflow

```
① Register pin for each LED           leds[i].begin(pin)
② Pass array & start timer interrupt   LED.begin(leds, count)
③ Specify behavior                    LED[i].toggle(t) / LED[i].pwm(duty) etc.
─────────────────────────────────────────────────────────────────────────────
From here on, a 1ms timer interrupt automatically processes blinking cycles.
```

`LED` is a **pre-instantiated global manager object** provided by the library (you do not need to construct it manually).

---

## class LEDClass — Single LED Control

### `LEDClass()` (Constructor)
* **Description**: Called automatically when an LED instance is created to initialize its internal states.
* **Note**: Sets the internal pin number to `255` (uninitialized marker). Always call `begin()` before active use.

### `void begin(uint8_t pin)`
* **Description**: Registers the target pin and sets its mode to `OUTPUT`. Should be called once during setup.
* **Parameter**: `pin` — GPIO pin number connected to the LED.
* **Behavior**: On AVR architecture, pre-fetches register addresses for direct port manipulation. Initializes with the LED in the **OFF** state.
* **Example**:
  ```cpp
  leds[0].begin(2);
  ```

### `void toggle(int t)`
* **Description**: Toggles state between ON and OFF every $t$ milliseconds (blinking).
* **Parameter**: `t` — Toggle interval in milliseconds ($[ms]$). **Passing `t <= 0` stops blinking**.
* **Note**: `t` specifies the *toggle interval*. `toggle(500)` yields a 1-second period ($500\text{ ms ON} + 500\text{ ms OFF}$).
* **Internal Behavior**: Temporarily disables interrupts while updating configuration variables to prevent race conditions with the timer interrupt.
* **Example**:
  ```cpp
  LED[0].toggle(500);  // Slow blink
  LED[0].toggle(0);    // Stop blinking (freezes current state)
  ```

### `void pwm(uint8_t duty)`
* **Description**: Adjusts LED brightness using PWM (wraps `analogWrite`).
* **Parameter**: `duty` — Brightness value from `0` (OFF) to `255` (Full ON).
* **Side Effect**: Calling this method automatically cancels active blinking (`toggle`) on this pin.
* **⚠️ Pin Restrictions (AVR only)**: Because this library uses `Timer1` for timekeeping interrupts, **`pwm()` will not work on `Timer1` pins (Pins 9 and 10)**. Use **Pins 3, 5, 6, or 11** on AVR boards. ESP32 series boards can use any output pin.
* **Example**:
  ```cpp
  LED[2].pwm(128);  // ~50% brightness
  ```

### `void setHighInline()`
* **Description**: Immediately turns the LED **ON**.
* **Implementation**: High-speed direct port manipulation using inline assembly on AVR; uses `digitalWrite(pin, HIGH)` on other platforms.

### `void setLowInline()`
* **Description**: Immediately turns the LED **OFF**.
* **Implementation**: High-speed direct port manipulation using inline assembly on AVR; uses `digitalWrite(pin, LOW)` on other platforms.

### `void _tick1ms()` *(Internal Use)*
* **Description**: Invoked every 1ms from the timer interrupt service routine (ISR) to track toggle timing intervals.
* **Note**: Not intended for direct invocation by end users (indicated by the leading underscore `_`).

---

## class LEDManager — Multi-LED Controller

Pre-instantiated by the library as a single global object named `LED`.

### `void begin(LEDClass* ledArray, size_t count)`
* **Description**: Registers the target `LEDClass` array and its element count, then **starts the 1ms hardware timer interrupt**.
* **Parameters**:
  * `ledArray` — Pointer to the array of `LEDClass` objects (you can pass the array variable name directly).
  * `count` — Number of LED instances in the array.
* **Example**:
  ```cpp
  LEDClass leds[3];
  LED.begin(leds, 3);
  ```

### `LEDClass& operator[](size_t index)`
* **Description**: Subscript operator allowing direct array-style element access like `LED[0]`.
* **Parameter**: `index` — 0-based index of the target LED.
* **Safety**: Out-of-bounds access or calls made prior to `begin()` are safely guarded to prevent crashes (out-of-bounds indices fall back to returning element 0).
* **Example**:
  ```cpp
  LED[1].toggle(200);
  ```

### `void initTimer1()` *(Internal Use)*
* **Description**: Sets up the 1ms hardware timer interrupt (automatically called inside `begin()`).
* **Implementation Details**:
  * **AVR**: Configures `Timer1` in CTC mode with a 64 prescaler targeting 1kHz.
  * **ESP32**: Configures a general hardware timer at 1MHz resolution triggering every $1000\text{ }\mu s$. (Compatible with both `arduino-esp32` v2.x and v3.x core APIs).

### `void _tickAll()` *(Internal Use)*
* **Description**: Called directly from the ISR to run `_tick1ms()` across all registered LED instances.

---

## Global Variables

### `extern LEDManager LED;`
* The shared manager instance across the application. Defined in `MyLED.cpp`.
* End users interact with managed LEDs through this object (e.g., `LED.begin(...)`, `LED[i]...`).

---

## Frequently Asked Questions

**Q. What is the difference between `LED` and `leds`?**
A. `leds` is the user-created array of individual LED components, while `LED` is the library's built-in manager (controller). Passing `LED.begin(leds, 3)` registers those components with the central manager.

**Q. Do I really leave `loop()` completely empty?**
A. Yes. Blinking cycles are managed in the background by timer interrupts, allowing you to use `loop()` for other application logic.

**Q. Can I blink an LED while adjusting its brightness (PWM)?**
A. Currently, `pwm()` and `toggle()` are mutually exclusive (calling one halts the other). This trade-off prioritizes architectural clarity as an educational project.

**Q. What do functions starting with an underscore (`_`) mean?**
A. Underscores denote private/internal library methods. They should not be called in user sketch code.