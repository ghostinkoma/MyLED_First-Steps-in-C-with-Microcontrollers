/*
 * ============================================================================
 *  MyLED.cpp  ―  MyLED.h 的“具体实现”
 * ============================================================================
 *  如果头文件 (.h) 是“声明（能做什么）”，那么这里就是“定义（怎么做）”。
 *
 *  针对不同单片机的处理逻辑，通过 #if 分为以下 3 个体系进行切换：
 *     ・AVR    (__AVR__)   … 包括 UNO/Nano/LGT8F。使用 Timer1 + 内联汇编
 *     ・ESP32 (ESP32)     … 包括 C3/S2/S3。使用硬件定时器 + digitalWrite
 *     ・其他               … 使用 digitalWrite 的安全回退方案（Fallback）
 * ============================================================================
 */

#include "MyLED.h"

// 在整个程序中仅存在一个的管理类的“实体”。
// （头文件中仅使用 extern 声明了“在某处存在”，实体在此处放置一个）
LEDManager LED;

// ---- 用于 ESP32: 硬件定时器句柄（AVR 中不使用）----
#if defined(ESP32)
static hw_timer_t* s_hwTimer = nullptr;
#endif


/* ===========================================================================
 *  LEDClass 的实现
 * =========================================================================== */

// 构造函数: 将所有状态统一初始化为“安全初始值”。
// _pin = 255 表示“尚未调用 begin()”的哨兵值（不存在的引脚号）。
LEDClass::LEDClass()
    : _pin(255), _port(nullptr), _bitmask(0),
      _toggle_interval_ms(0), _timer_counter(0),
      _toggle_enabled(false), _state(false) {}

// 注册所使用的引脚，并将其设置为输出模式。
void LEDClass::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);

#if defined(__AVR__)
    // 在 AVR 中，为了“直接操作端口”，根据引脚号预先获取：
    //    _port    … 该引脚所属输出寄存器的地址
    //    _bitmask … 该引脚在该寄存器中所占用的位
    // 这样可以实现比 digitalWrite 更快的点亮/熄灭操作。
    _port    = portOutputRegister(digitalPinToPort(_pin));
    _bitmask = digitalPinToBitMask(_pin);
#endif
    // 在 ESP32 等芯片中 _port 保持为 nullptr → 在下方的 setLow/HighInline() 中
    // 会自动使用 digitalWrite。

    setLowInline(); // 初始状态务必从熄灭开始
}

// ---- 从这里开始的下方 2 个 free 函数是用于“AVR 汇编演示”的辅助函数 ----
//      加上 static 使其成为仅在本文件中使用的函数。
#if defined(__AVR__)
// HIGH（点亮）: 仅将端口对应的位设为 1（保持其他位不变）
static inline void setHighInlineImpl(volatile uint8_t* port, uint8_t mask) {
    __asm__ __volatile__ (
        "ld  __tmp_reg__, Z    \n\t" // 将 Z(=port) 所指向的当前值读取到临时寄存器
        "or  __tmp_reg__, %0   \n\t" // 与 mask 进行按位或运算（将对应位置 1）
        "st  Z, __tmp_reg__    \n\t" // 写回端口
        :
        : "r" (mask), "z" (port)
        : "memory"
    );
}
// LOW（熄灭）: 仅将端口对应的位设为 0（保持其他位不变）
static inline void setLowInlineImpl(volatile uint8_t* port, uint8_t mask) {
    uint8_t inv_mask = ~mask;                // 将 mask 取反（仅对应位为 0，其他位为 1）
    __asm__ __volatile__ (
        "ld  __tmp_reg__, Z    \n\t"
        "and __tmp_reg__, %0   \n\t"         // 与反转后的 mask 进行按位与运算（将对应位置 0）
        "st  Z, __tmp_reg__    \n\t"
        :
        : "r" (inv_mask), "z" (port)
        : "memory"
    );
}
#endif // __AVR__

// 点亮。AVR 使用高速汇编，其他微控制器直接使用 digitalWrite。
void LEDClass::setHighInline() {
#if defined(__AVR__)
    if (_port) { setHighInlineImpl(_port, _bitmask); return; }
#endif
    digitalWrite(_pin, HIGH);
}

// 熄灭。同上。
void LEDClass::setLowInline() {
#if defined(__AVR__)
    if (_port) { setLowInlineImpl(_port, _bitmask); return; }
#endif
    digitalWrite(_pin, LOW);
}

// 设置每 t 毫秒闪烁一次。当 t<=0 时停止闪烁。
void LEDClass::toggle(int t) {
    // 这些变量也会在定时器中断 (_tick1ms) 中被读取和写入。
    // 如果在修改多个变量的“过程中”触发中断，可能会导致数据错乱，
    // 因此在修改期间暂时关闭中断以实现安全更新（原子更新）。
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

// PWM（亮度调节）。0=熄灭 〜 255=全亮。
void LEDClass::pwm(uint8_t duty) {
    _toggle_enabled = false; // PWM 期间停止闪烁翻转
#if defined(__AVR__)
    // 【注意 / 仅限 AVR】由于本库占用了 Timer1 用于闪烁，
    // 连接到 Timer1 的 9、10 号引脚上的 analogWrite 将无法正常工作。
    // 用于 PWM 的引脚请选择 3、5、6、11 号（Timer0/Timer2）。
#endif
    analogWrite(_pin, duty);
}

// 由定时器中断每 1 毫秒调用一次。这里是闪烁逻辑的核心。
void LEDClass::_tick1ms() {
    if (!_toggle_enabled || _toggle_interval_ms == 0) return;

    _timer_counter++;
    if (_timer_counter >= _toggle_interval_ms) {
        _timer_counter = 0;
        _state = !_state;                 // 翻转亮灭状态
        if (_state) setHighInline();
        else        setLowInline();
    }
}


/* ===========================================================================
 *  LEDManager 的实现
 * =========================================================================== */

LEDManager::LEDManager() : _leds(nullptr), _count(0) {}

// 注册管理的 LED 数组并启动 1ms 定时器。
void LEDManager::begin(LEDClass* ledArray, size_t count) {
    _leds  = ledArray;
    _count = count;
    initTimer1();
}

// 下标运算符，支持 LED[n] 形式的访问。越界或未初始化时返回 0 号以防止程序崩溃。
LEDClass& LEDManager::operator[](size_t index) {
    if (_leds == nullptr) {
        // 在 begin() 之前被调用时的保护措施。返回静态伪对象以防止崩溃。
        static LEDClass dummy;
        return dummy;
    }
    if (index >= _count) index = 0; // 越界访问保护
    return _leds[index];
}

// 批量执行所有 LED 的 1ms 处理逻辑（由中断调用）。
void LEDManager::_tickAll() {
    for (size_t i = 0; i < _count; i++) {
        _leds[i]._tick1ms();
    }
}


/* ===========================================================================
 *  1ms 定时器中断设置（此处为依赖单片机硬件的核心逻辑）
 * =========================================================================== */

#if defined(__AVR__)
// ---- AVR（UNO/Nano/LGT8F）: 将 Timer1 设置为 CTC 模式，频率 1kHz(=1ms) ----
void LEDManager::initTimer1() {
    noInterrupts();          // 设置期间暂停中断
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // 比较值 OCR1A 根据 F_CPU 自动计算（16MHz 下为 249）。
    //    中断频率 = F_CPU / 预分频器(64) / (OCR1A + 1) = 1000Hz
    OCR1A  = (uint16_t)(F_CPU / 64UL / 1000UL - 1UL);

    TCCR1B |= (1 << WGM12);              // CTC 模式（达到 OCR1A 时自动重置）
    TCCR1B |= (1 << CS11) | (1 << CS10); // 预分频器 64
    TIMSK1 |= (1 << OCIE1A);             // 允许比较匹配 A 中断
    interrupts();
}

// AVR 的中断服务程序（ISR）。每 1ms 自动调用一次。
ISR(TIMER1_COMPA_vect) {
    LED._tickAll();
}

#elif defined(ESP32)
// ---- ESP32 / C3 / S2 / S3: 使用通用硬件定时器实现 1ms 周期中断 ----
// ※ 中断处理函数需要放置在 RAM 中，因此加上 IRAM_ATTR 属性。
void IRAM_ATTR onLedTimer() {
    LED._tickAll();
}

void LEDManager::initTimer1() {
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // arduino-esp32 3.x 及以上版本的新 API。创建 1MHz（=1μs步进）的定时器。
    s_hwTimer = timerBegin(1000000);              // 分辨率 1MHz
    timerAttachInterrupt(s_hwTimer, &onLedTimer); // 注册中断函数
    timerAlarm(s_hwTimer, 1000, true, 0);          // 每 1000μs=1ms 触发一次，自动重载
  #else
    // arduino-esp32 2.x 及更早版本的旧 API（为了兼容性而保留）。
    s_hwTimer = timerBegin(0, 80, true);          // 80MHz/80 = 1MHz
    timerAttachInterrupt(s_hwTimer, &onLedTimer, true);
    timerAlarmWrite(s_hwTimer, 1000, true);       // 每 1ms 触发一次
    timerAlarmEnable(s_hwTimer);
  #endif
}

#else
// ---- 上述以外的单片机: 无定时器的回退方案（Fallback） ----
// （适用于无法提供定时器中断的环境。闪烁功能将无法运行，
//    但 setHighInline()/setLowInline()/pwm() 仍可正常使用。）
void LEDManager::initTimer1() {
    // 不执行任何操作
}
#endif