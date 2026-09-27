/*
 * ============================================================================
 *  MyLED.h  ―  使用“中断定时器”同时控制多个 LED 的教学用库
 * ============================================================================
 *
 *  【通过本库可以学到什么】
 *   1. 类（class）的编写方法 … 将单个 LED 抽象为 LEDClass，将整体管理抽象为 LEDManager
 *   2. 数组与指针            … 批量处理多个 LED
 *   3. 运算符重载            … 实现类似 LED[0] 这种“像数组一样”访问的机制
 *   4. 硬件定时器中断        … 不阻塞 loop() 的情况下按固定时间间隔进行处理
 *   5.（进阶）AVR 内联汇编   … 直接操作端口以实现高速化
 *
 *  【支持的单片机】
 *   - Arduino UNO / Nano 等（ATmega328P, 16MHz）… Timer1 + 内联汇编
 *   - LGT8F328P                                 … 同上（兼容 AVR）
 *   - ESP32 / ESP32-C3 / ESP32-S2 / ESP32-S3     … 硬件定时器 + digitalWrite
 *
 *  “不同单片机之间的差异”通过 #if defined(...) 自动切换。
 *  使用者侧（.ino）的代码在任何单片机上均可完全相同地运行。
 * ============================================================================
 */

#ifndef MY_LED_H          // ← 防止多次加载同一个头文件导致重复定义的“Include Guard（包含保护）”
#define MY_LED_H

#include <Arduino.h>      // pinMode / digitalWrite / uint8_t 等基本功能

/* ---------------------------------------------------------------------------
 *  LEDClass : 表示“单个” LED 的类
 *
 *  针对单个 LED，保存“连接到哪个引脚”、“是否闪烁”、“当前是否点亮”等
 *  信息（状态），并提供点亮 / 熄灭 / 闪烁等操作功能。
 * ------------------------------------------------------------------------- */
class LEDClass {
private:
    // ---- 引脚相关信息 ----
    uint8_t _pin;              // 连接该 LED 的引脚号

    // ---- 用于在 AVR 中“直接操作端口”的信息（ESP32 中未使用）----
    volatile uint8_t* _port;   // 输出端口寄存器的地址（例如: &PORTB）
    uint8_t _bitmask;          // 该引脚在该端口中所占用的位（例如: 0x20）

    // ---- 闪烁（Toggle）状态。由于会在中断中被修改，因此加上 volatile 关键字 ----
    volatile uint16_t _toggle_interval_ms; // 每隔多少毫秒翻转一次（0=不闪烁）
    volatile uint16_t _timer_counter;      // 记录已过毫秒数的计数器
    volatile bool      _toggle_enabled;     // 当前是否正在闪烁
    volatile bool      _state;              // 当前是 HIGH(点亮) 还是 LOW(熄灭)

public:
    LEDClass();                 // 构造函数（创建对象时调用的初始化）

    // 注册使用的引脚并设置为输出模式。最开始只需调用 1 次
    void begin(uint8_t pin);

    // 点亮 / 熄灭 LED（AVR 中使用汇编，其他微控制器使用 digitalWrite）
    void setHighInline();
    void setLowInline();

    // 每 t 毫秒重复点亮和熄灭。传入 t=0 可停止闪烁
    void toggle(int t);

    // 将亮度划分为 256 个等级，在 0(熄灭)〜255(全亮) 之间指定（PWM）
    void pwm(uint8_t duty);

    // 【内部使用】由定时器中断每 1 毫秒调用一次。用户请勿直接调用
    void _tick1ms();
};

/* ---------------------------------------------------------------------------
 *  LEDManager : 对多个 LEDClass 进行“统一管理”的类
 *
 *  ・接收并保存 LED 数组
 *  ・实现类似 LED[0] 这样通过下标进行访问的功能（operator[]）
 *  ・仅启动 1 个 1ms 的定时器中断，并统一调用所有 LED 的 _tick1ms()
 *
 *  该类的实例（实体）如随后的 extern 声明所示，已经以 LED 为名
 *  准备好了一个。用户直接使用该实例即可。
 * ------------------------------------------------------------------------- */
class LEDManager {
private:
    LEDClass* _leds;   // 所管理的 LED 数组的首地址
    size_t    _count;  // LED 的数量

public:
    LEDManager();

    // 注册所管理的 LED 数组及其数量，并启动定时器中断
    void begin(LEDClass* ledArray, size_t count);

    // 支持 LED[n] 形式的语法（下标运算符重载）
    LEDClass& operator[](size_t index);

    // 【内部使用】1ms 定时器的设置
    void initTimer1();

    // 【内部使用】由中断调用，批量调用所有 LED 的 _tick1ms()
    void _tickAll();
};

// 整个程序中共享的唯一管理者。实体位于 MyLED.cpp 中
extern LEDManager LED;

#endif // MY_LED_H