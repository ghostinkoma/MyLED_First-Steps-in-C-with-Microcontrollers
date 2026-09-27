# MyLED Class Design Specification

This document details the **class design (internal architecture)** of the MyLED library.
Read this to understand "why it was built this way" and "which class handles what responsibility."

---

## 1. Design Principles

| Principle | Details |
|---|---|
| **Separation of Concerns** | Separate single-LED management (`LEDClass`) from multi-LED control (`LEDManager`). |
| **Non-blocking** | Handle blinking via timer interrupts instead of `delay()`, leaving `loop()` completely free. |
| **Intuitive Syntax** | Enable array-like access using operator overloading (e.g., `LED[0].toggle(500)`). |
| **Portability** | Encapsulate platform-dependent code inside `#if defined(...)` so sketch code remains uniform. |

---

## 2. Class Diagram

```
                 ┌─────────────────────────────┐
                 │         LEDManager  (LED)   │  Controller (Singleton-like)
                 │  - _leds : LEDClass*        │
                 │  - _count : size_t          │
                 │  + begin(array, count)      │
                 │  + operator[](i) ───────────┐│
                 │  + initTimer1() / _tickAll()││
                 └──────────────────────────────┘│
                         │ Manages (Holds array) │ Access via LED[i]
                         ▼                       ▼
     ┌───────────┐  ┌───────────┐  ┌───────────┐
     │ LEDClass  │  │ LEDClass  │  │ LEDClass  │  Components (Any quantity)
     │  leds[0]  │  │  leds[1]  │  │  leds[2]  │
     └───────────┘  └───────────┘  └───────────┘

  [1ms Timer Interrupt] → LED._tickAll() → each leds[i]._tick1ms()
```

---

## 3. Class Specification: `LEDClass` (Single LED)

**Role**: Holds the state of an individual LED and handles turning it ON/OFF, blinking, and dimming (PWM).

### Member Variables

| Variable | Type | Access | Description |
|---|---|---|---|
| `_pin` | `uint8_t` | `private` | Target GPIO pin number (`255` = uninitialized). |
| `_port` | `volatile uint8_t*` | `private` | Address of output port register (AVR only). |
| `_bitmask` | `uint8_t` | `private` | Bitmask position inside the port (AVR only). |
| `_toggle_interval_ms` | `volatile uint16_t` | `private` | Toggle interval in ms (`0` = disabled). |
| `_timer_counter` | `volatile uint16_t` | `private` | Elapsed millisecond counter. |
| `_toggle_enabled` | `volatile bool` | `private` | Toggle active state flag. |
| `_state` | `volatile bool` | `private` | Current logical state (`HIGH` / `LOW`). |

> `volatile` tells the compiler that the variable can be modified inside interrupt routines, preventing undesirable optimization steps.

### Member Functions

| Method | Access | Responsibility |
|---|---|---|
| `LEDClass()` | `public` | Initializes internal flags to safe default values. |
| `begin(uint8_t pin)` | `public` | Registers pin, sets mode to `OUTPUT`, and caches port addresses (AVR). |
| `setHighInline()` | `public` | Turns LED ON immediately (Assembly on AVR / `digitalWrite` elsewhere). |
| `setLowInline()` | `public` | Turns LED OFF immediately (Assembly on AVR / `digitalWrite` elsewhere). |
| `toggle(int t)` | `public` | Configures blinking every $t$ ms (`0` stops blinking). |
| `pwm(uint8_t duty)` | `public` | Adjusts brightness using PWM ($0$–$255$). |
| `_tick1ms()` | `public` (Internal) | Called every 1ms from timer ISR to progress blinking cycles. |

---

## 4. Class Specification: `LEDManager` (Manager)

**Role**: Aggregates multiple `LEDClass` instances and runs a single hardware timer interrupt to drive all registered LEDs.
A single global instance named `LED` is instantiated (acting as a singleton).

### Member Variables

| Variable | Type | Access | Description |
|---|---|---|---|
| `_leds` | `LEDClass*` | `private` | Pointer to the head of the managed `LEDClass` array. |
| `_count` | `size_t` | `private` | Number of LEDs managed. |

### Member Functions

| Method | Access | Responsibility |
|---|---|---|
| `LEDManager()` | `public` | Initializes internal array pointer and counter. |
| `begin(LEDClass*, size_t)` | `public` | Binds array reference and invokes `initTimer1()`. |
| `operator[](size_t)` | `public` | Array subscript access (`LED[i]`) with bounds safety. |
| `initTimer1()` | `public` (Internal) | Configures and starts the 1ms timer interrupt. |
| `_tickAll()` | `public` (Internal) | Calls `_tick1ms()` on every registered `LEDClass` instance. |

---

## 5. Execution Sequence

### Initialization Phase
```
setup()
 ├─ leds[i].begin(pin)         … Configure pin modes & turn off LEDs
 └─ LED.begin(leds, count)
        └─ initTimer1()        … Start 1ms hardware timer interrupt
```

### Runtime Phase (Automatic 1ms Cycle)
```
[Timer Interrupt Triggers]
 └─ LED._tickAll()
      └─ each leds[i]._tick1ms()
           └─ When counter reaches target: invert _state → call setHigh/LowInline()
```

---

## 6. Platform Abstraction

Only timer control and direct pin manipulation differ by platform; higher-level logic remains identical.

| Feature | AVR (`__AVR__`) | ESP32 (`ESP32`) | Other Architectures |
|---|---|---|---|
| **Timer Service** | `Timer1` CTC / `ISR(TIMER1_COMPA_vect)` | `hw_timer_t` + `IRAM_ATTR` ISR | None (Blinking disabled) |
| **I/O Control** | Inline Assembly (Direct Port Access) | `digitalWrite` | `digitalWrite` |
| **ESP32 API Support**| — | v3.x (`timerBegin` + `timerAlarm`) / v2.x (`timerAlarmWrite`) | — |

> Conditional compilation is managed using `#if defined(__AVR__) / #elif defined(ESP32) / #else` inside `MyLED.cpp`. No modifications are needed in user sketches (`.ino`).

---

## 7. Known Limitations & Trade-offs

| Limitation | Reason / Workaround |
|---|---|
| **No `pwm()` on pins 9/10 (AVR)** | `Timer1` is dedicated to timing interrupts. Use pins 3, 5, 6, or 11 instead. |
| **`pwm()` & `toggle()` are mutually exclusive** | Kept simple for educational clarity. Calling one halts the other. |
| **1ms timer resolution** | Toggle timing is bounded by the 1ms tick interrupt rate. |
| **`digitalWrite` used inside ESP32 ISR** | Prioritizes clarity over pure speed. Direct register writing is faster but more complex. |

---

## 8. Glossary

- **Class / Instance**: The blueprint (`class`) versus the concrete object created from it (`instance`).
- **`private` / `public`**: Internal private members vs. methods exposed to external user code.
- **Operator Overloading**: Defining custom behaviors for syntax symbols like `[]`.
- **Timer Interrupt**: Hardware trigger that periodically pauses main code execution to run a routine.
- **`volatile`**: Qualifier instructing compiler optimizations not to cache memory reads for interrupt-shared variables.
- **CTC Mode**: Clear Timer on Compare Match; an AVR timer mode that automatically resets counter to 0 upon reaching target.