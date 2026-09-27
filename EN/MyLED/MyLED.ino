/*
 * ============================================================================
 *  MyLED.ino  ―  MyLED Library Usage Example
 * ============================================================================
 *  Controls 3 LEDs simultaneously with different patterns using timer interrupts.
 *  `loop()` contains no code. The key feature of this library is that blinking
 *  is handled automatically by background timer interrupts without blocking
 *  the main loop.
 *
 *  Wiring Example:
 *    MCU Pin --[Resistor 220–330Ω]-- LED Anode(+) … LED Cathode(-) -- GND
 *
 *  ★ Pin Selection Guidelines ★
 *    - Blinking (toggle): Any standard output pin can be used.
 *    - Dimming (pwm): Pay attention to pin choices:
 *        AVR (UNO/Nano): Use pins 3, 5, 6, or 11 (pins 9 and 10 conflict with Timer1).
 *        ESP32 series: Any available output GPIO pin can be used.
 * ============================================================================
 */

#include "src/MyLED.h"

// Instantiate 3 LED objects (array)
LEDClass leds[3];

void setup() {
    // 1) Register the GPIO pin for each LED (configures pin mode to OUTPUT).
    //    Change the pin numbers below to match your actual wiring setup.
    leds[0].begin(2);   // For blinking
    leds[1].begin(4);   // For blinking
    leds[2].begin(11);  // For PWM (Pin 11 avoids Timer1 conflicts on AVR)

    // 2) Pass the array pointer and count to the manager. Starts 1ms timer interrupt.
    LED.begin(leds, 3);

    // 3) Specify individual LED behavior
    LED[0].toggle(500); // Toggle state every 500ms (slow blink)
    LED[1].toggle(200); // Toggle state every 200ms (fast blink)
    LED[2].pwm(128);    // Set brightness to 50% (midpoint of 0–255)
}

void loop() {
    // Nothing required here. Blinking is handled automatically by timer interrupts,
    // leaving main loop free to focus on other tasks (e.g., sensor readings).
}