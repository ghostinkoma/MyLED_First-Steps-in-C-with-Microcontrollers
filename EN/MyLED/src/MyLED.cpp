/*
 * ============================================================================
 *  MyLED.cpp  ―  Implementation of MyLED.h
 * ============================================================================
 *  While the header (.h) provides the declarations ("what it can do"),
 *  this file provides the definitions ("how it works").
 *
 *  Microcontroller-specific behavior is selected via preprocessor directives (#if):
 *    - AVR     (__AVR__)     ... UNO/Nano/LGT8F. Uses Timer1 + Inline Assembly
 *    - ESP32   (ESP32)       ... Includes C3/S2/S3. Uses HW Timer + digitalWrite
 *    - Others                ... Safe fallback using digitalWrite
 * ============================================================================
 */

#include "MyLED.h"

// Global singleton instance declaration for LEDManager.
// (Declared as extern in the header; defined once here in the implementation file.)
LEDManager LED;

// ---- ESP32-specific: Hardware timer handle (unused on AVR) ----
#if defined(ESP32)
static hw_timer_t* s_hwTimer = nullptr;
#endif


/* ===========================================================================
 *  LEDClass Implementation
 * =========================================================================== */

// Constructor: Initializes all internal state variables to safe defaults.
// _pin = 255 acts as a sentinel value indicating "begin() not yet called".
LEDClass::LEDClass()
    : _pin(255), _port(nullptr), _bitmask(0),
      _toggle_interval_ms(0), _timer_counter(0),
      _toggle_enabled(false), _state(false) {}

// Registers the designated pin and configures it as OUTPUT mode.
void LEDClass::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);

#if defined(__AVR__)
    // On AVR, pre-calculate the port address and bitmask to enable fast bitwise access:
    //    _port    ... Address of the output register for this pin
    //    _bitmask ... Bit position of this pin within the register
    // This allows faster state toggling compared to standard digitalWrite().
    _port    = portOutputRegister(digitalPinToPort(_pin));
    _bitmask = digitalPinToBitMask(_pin);
#endif
    // On ESP32 and other architectures, _port remains nullptr.
    // setLowInline()/setHighInline() will fall back to standard digitalWrite().

    setLowInline(); // Ensure pin starts in LOW state (turned off)
}

// ---- The two helper functions below provide inline assembly for AVR ----
//      Marked as static to limit visibility to this translation unit.
#if defined(__AVR__)
// HIGH (Turn ON): Sets the target pin bit to 1 while preserving all other bits in the register.
static inline void setHighInlineImpl(volatile uint8_t* port, uint8_t mask) {
    __asm__ __volatile__ (
        "ld  __tmp_reg__, Z    \n\t" // Load current port byte from register address Z into temp reg
        "or  __tmp_reg__, %0   \n\t" // Bitwise OR with mask (sets the specified bit to 1)
        "st  Z, __tmp_reg__    \n\t" // Store updated byte back to port
        :
        : "r" (mask), "z" (port)
        : "memory"
    );
}
// LOW (Turn OFF): Clears the target pin bit to 0 while preserving all other bits in the register.
static inline void setLowInlineImpl(volatile uint8_t* port, uint8_t mask) {
    uint8_t inv_mask = ~mask;                 // Invert mask (target bit = 0, all other bits = 1)
    __asm__ __volatile__ (
        "ld  __tmp_reg__, Z    \n\t"
        "and __tmp_reg__, %0   \n\t"          // Bitwise AND with inverted mask (clears target bit to 0)
        "st  Z, __tmp_reg__    \n\t"
        :
        : "r" (inv_mask), "z" (port)
        : "memory"
    );
}
#endif // __AVR__

// Turn LED ON using inline assembly on AVR, or standard digitalWrite on other architectures.
void LEDClass::setHighInline() {
#if defined(__AVR__)
    if (_port) { setHighInlineImpl(_port, _bitmask); return; }
#endif
    digitalWrite(_pin, HIGH);
}

// Turn LED OFF using inline assembly on AVR, or standard digitalWrite on other architectures.
void LEDClass::setLowInline() {
#if defined(__AVR__)
    if (_port) { setLowInlineImpl(_port, _bitmask); return; }
#endif
    digitalWrite(_pin, LOW);
}

// Configures toggle interval in milliseconds. Setting t <= 0 disables toggling.
void LEDClass::toggle(int t) {
    // Variables updated here are accessed concurrently inside the timer ISR (_tick1ms).
    // To prevent race conditions during multi-variable updates, interrupt execution is temporarily suspended (atomic section).
    noInterrupts();
    if (t <= 0) {
        _toggle_enabled     = false;
        _toggle_interval_ms = 0;
    } else {
        _toggle_interval_ms = (uint16_t)t;
        _timer_counter      = 0;
        _toggle_enabled     = true;
    }
    interrupts();
}

// Sets PWM brightness level (0 = OFF, 255 = Full Brightness).
void LEDClass::pwm(uint8_t duty) {
    _toggle_enabled = false; // Disable non-blocking toggle mode when using PWM
#if defined(__AVR__)
    // [Note for AVR platforms] Timer1 is allocated for the library's non-blocking interval interrupt.
    // Consequently, analogWrite() on Timer1 pins (Pins 9 & 10) will not function properly.
    // Use Pins 3, 5, 6, or 11 (Timer0 / Timer2) when PWM output is required on AVR.
#endif
    analogWrite(_pin, duty);
}

// Executed every 1 millisecond inside the timer ISR. Handles non-blocking LED state updates.
void LEDClass::_tick1ms() {
    if (!_toggle_enabled || _toggle_interval_ms == 0) return;

    _timer_counter++;
    if (_timer_counter >= _toggle_interval_ms) {
        _timer_counter = 0;
        _state = !_state;                 // Toggle state flag
        if (_state) setHighInline();
        else        setLowInline();
    }
}


/* ===========================================================================
 *  LEDManager Implementation
 * =========================================================================== */

LEDManager::LEDManager() : _leds(nullptr), _count(0) {}

// Registers the array of managed LED objects and initializes the 1ms system timer.
void LEDManager::begin(LEDClass* ledArray, size_t count) {
    _leds  = ledArray;
    _count = count;
    initTimer1();
}

// Subscript operator providing array-like indexing syntax (LED[n]).
// Includes safety checks for out-of-bounds access or uninitialized calls.
LEDClass& LEDManager::operator[](size_t index) {
    if (_leds == nullptr) {
        // Fallback protection if accessed prior to calling begin().
        static LEDClass dummy;
        return dummy;
    }
    if (index >= _count) index = 0; // Prevent out-of-bounds memory access
    return _leds[index];
}

// Invokes _tick1ms() across all registered LED instances (called inside ISR).
void LEDManager::_tickAll() {
    for (size_t i = 0; i < _count; i++) {
        _leds[i]._tick1ms();
    }
}


/* ===========================================================================
 *  1ms Timer Interrupt Setup (Platform-Specific Core)
 * =========================================================================== */

#if defined(__AVR__)
// ---- AVR (UNO/Nano/LGT8F): Configures Timer1 in CTC mode for 1kHz (1ms) operation ----
void LEDManager::initTimer1() {
    noInterrupts();          // Disable interrupts during timer configuration
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // Calculate OCR1A compare match register value based on F_CPU (e.g., 249 for 16MHz clock).
    // Target Frequency = F_CPU / Prescaler(64) / (OCR1A + 1) = 1000Hz
    OCR1A  = (uint16_t)(F_CPU / 64UL / 1000UL - 1UL);

    TCCR1B |= (1 << WGM12);              // CTC mode (Clear Timer on Compare Match)
    TCCR1B |= (1 << CS11) | (1 << CS10); // Set prescaler to 64
    TIMSK1 |= (1 << OCIE1A);             // Enable Timer1 compare match A interrupt
    interrupts();
}

// AVR Timer1 Compare Match A Interrupt Service Routine (called every 1ms).
ISR(TIMER1_COMPA_vect) {
    LED._tickAll();
}

#elif defined(ESP32)
// ---- ESP32 / C3 / S2 / S3: Uses hardware timer for 1ms recurring interrupt ----
// ISR function must reside in Internal RAM (IRAM), specified via IRAM_ATTR.
void IRAM_ATTR onLedTimer() {
    LED._tickAll();
}

void LEDManager::initTimer1() {
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // Modern API for arduino-esp32 v3.x+. Configures timer base frequency at 1MHz (1μs tick).
    s_hwTimer = timerBegin(1000000);              // 1MHz timer resolution
    timerAttachInterrupt(s_hwTimer, &onLedTimer); // Attach ISR handler
    timerAlarm(s_hwTimer, 1000, true, 0);          // Trigger every 1000μs (1ms), autoreload
  #else
    // Legacy API for arduino-esp32 v2.x (retained for backward compatibility).
    s_hwTimer = timerBegin(0, 80, true);          // 80MHz / 80 prescaler = 1MHz
    timerAttachInterrupt(s_hwTimer, &onLedTimer, true);
    timerAlarmWrite(s_hwTimer, 1000, true);       // 1ms interval
    timerAlarmEnable(s_hwTimer);
  #endif
}

#else
// ---- Fallback for unsupported microcontrollers ----
// (For environments without timer interrupt implementations.
//  Automatic toggling will not function, but setHighInline()/setLowInline()/pwm() remain usable.)
void LEDManager::initTimer1() {
    // No-op fallback
}
#endif