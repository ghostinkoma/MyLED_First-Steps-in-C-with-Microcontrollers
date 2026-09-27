/*
 * ============================================================================
 *  MyLED.h  ―  Educational library for simultaneous LED control via timer interrupts
 * ============================================================================
 *
 *  【Learning Concepts Covered】
 *   1. Class Design           ... Encapsulating a single LED in LEDClass and whole management in LEDManager
 *   2. Arrays & Pointers      ... Managing multiple LED instances efficiently
 *   3. Operator Overloading   ... Enabling array-like subscript indexing syntax (e.g., LED[0])
 *   4. Hardware Timer ISR     ... Non-blocking background execution without halting loop()
 *   5. Advanced (AVR)         ... Direct register manipulation via inline assembly
 *
 *  【Supported Microcontrollers】
 *   - Arduino UNO / Nano etc. (ATmega328P, 16MHz) ... Timer1 + Inline Assembly
 *   - LGT8F328P                                   ... Same as above (AVR compatible)
 *   - ESP32 / ESP32-C3 / ESP32-S2 / ESP32-S3     ... Hardware Timer + digitalWrite
 *
 *  Microcontroller differences are abstracted via `#if defined(...)`.
 *  User sketch code (.ino) remains identical regardless of target hardware.
 * ============================================================================
 */

#ifndef MY_LED_H           // ← Include guard to prevent multiple inclusion errors
#define MY_LED_H

#include <Arduino.h>      // Core Arduino functions: pinMode, digitalWrite, uint8_t, etc.

/* ---------------------------------------------------------------------------
 *  LEDClass : Class representing a single LED instance
 *
 *  Encapsulates pin information, toggling state, and active output level for one LED,
 *  providing methods for toggling, state switching, and PWM output.
 * ------------------------------------------------------------------------- */
class LEDClass {
private:
    // ---- Pin Identification ----
    uint8_t _pin;               // Assigned GPIO pin number for this LED

    // ---- AVR Direct Port Access (Unused on ESP32) ----
    volatile uint8_t* _port;   // Output port register address (e.g., &PORTB)
    uint8_t _bitmask;          // Bitmask corresponding to this pin within the port (e.g., 0x20)

    // ---- Toggle state variables modified inside ISR (declared volatile) ----
    volatile uint16_t _toggle_interval_ms; // Toggle state inversion interval in ms (0 = disabled)
    volatile uint16_t _timer_counter;      // Counter tracking elapsed milliseconds
    volatile bool      _toggle_enabled;     // Flag indicating whether non-blocking toggle is active
    volatile bool      _state;              // Current logic state: HIGH (ON) or LOW (OFF)

public:
    LEDClass();                 // Constructor initializing state variables

    // Assigns pin number and configures output direction (called once at startup)
    void begin(uint8_t pin);

    // Drives LED output HIGH or LOW (uses inline assembly on AVR, digitalWrite elsewhere)
    void setHighInline();
    void setLowInline();

    // Toggles state every t milliseconds (pass t <= 0 to stop toggling)
    void toggle(int t);

    // Sets PWM duty cycle level from 0 (OFF) to 255 (Fully ON)
    void pwm(uint8_t duty);

    // 【Internal】Called every 1 millisecond by timer ISR (do not call directly in sketch)
    void _tick1ms();
};

/* ---------------------------------------------------------------------------
 *  LEDManager : Central manager handling multiple LEDClass instances
 *
 *  - Holds reference to array of LED objects
 *  - Enables array indexing notation (e.g., LED[0]) via operator overloading
 *  - Runs a single 1ms timer interrupt driving _tick1ms() across all LEDs
 *
 *  A single global instance named `LED` is defined in `MyLED.cpp`.
 * ------------------------------------------------------------------------- */
class LEDManager {
private:
    LEDClass* _leds;   // Base address pointer to the array of managed LED objects
    size_t    _count;  // Total count of managed LEDs

public:
    LEDManager();

    // Registers the target LED array, element count, and initializes timer ISR
    void begin(LEDClass* ledArray, size_t count);

    // Subscript operator overload enabling LED[n] syntax
    LEDClass& operator[](size_t index);

    // 【Internal】Configures 1ms hardware timer interrupt
    void initTimer1();

    // 【Internal】Timer ISR entry point; dispatches _tick1ms() across all LEDs
    void _tickAll();
};

// Global singleton instance shared across application; defined in MyLED.cpp
extern LEDManager LED;

#endif // MY_LED_H