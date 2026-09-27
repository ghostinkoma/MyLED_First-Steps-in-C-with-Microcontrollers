# Stepping Up from C to C++

"I can write C code for Arduino, but when it comes to writing my own C++ class, my mind goes completely blank and I don't know where to start." — This repository was created specifically for anyone who has felt this way.

The goal of this repository is to help you truly grasp C++ concepts by running the code, seeing the results with your own eyes, and feeling a sense of familiarity—like saying, "Ah, I've used something like this in Arduino before!" For this reason, **intuitive understanding and clarity** were prioritized above all else in its design.

---

## Design Specifications and Code

When writing a program, the process starts with design: deciding "what kind of class do I want to build?" and organizing its members.

For this project, we selected a lean yet essential set of features so that writing code and seeing the resulting physical LED behavior would link together intuitively.

### Member Functions List

| Function | Role |
|---|---|
| `void begin(uint8_t pin)` | Pin registration and initialization setup |
| `void toggle(int t)` | Non-blocking LED toggling (every `t` milliseconds) |
| `void pwm(uint8_t duty)` | Brightness control via PWM (0 to 255) |
| `void setHighInline()` | High-speed LED activation (ON) via direct port manipulation |
| `void setLowInline()` | High-speed LED deactivation (OFF) via direct port manipulation |
| `void _tick1ms()` | Internal 1ms timer ISR handler to avoid relying on `delay()` |

---

## Leveraging Your C Experience

Moving beyond the blocking `delay()` approach commonly found in beginner Arduino sketches, this library is designed to naturally teach you how to use **Timer peripherals**—one of the most widely used features in real-world embedded development.

---

## Author's Thoughts

In environments backed by rich operating systems, massive memory, and abundant compute resources, handling tasks like this is trivial.
However, in practical embedded systems development, we are constantly fighting against resource limits. I feel every day that learning to utilize the hardware and system efficiently is the real key to achieving high responsiveness.

This library is intended to serve as a foundational building block for learning.
Please feel free to add your own ideas and extend it however you like!

---

## License & Message

This repository is **completely free for personal, educational, and commercial use**.

Nothing would bring me greater joy as an author than hearing your feedback, such as "Thanks to this material, I was able to take on new tasks at work," or "I managed to solve a tough challenge." When that day comes, perhaps I will be the one learning from you.

I sincerely hope that everyone who encounters this repository finds success and growth in their technical journey.