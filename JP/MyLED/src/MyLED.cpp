/*
 * ============================================================================
 *  MyLED.cpp  ―  MyLED.h の「中身（実装）」
 * ============================================================================
 *  ヘッダ(.h)が「宣言（何ができるか）」なら、こちらは「定義（どうやるか）」です。
 *
 *  マイコンごとに処理が分かれる部分は、次の3系統に分けて #if で切り替えます。
 *    ・AVR   (__AVR__)  … UNO/Nano/LGT8F。Timer1 + インラインアセンブラ
 *    ・ESP32 (ESP32)    … C3/S2/S3 含む。ハードウェアタイマー + digitalWrite
 *    ・その他            … digitalWrite による安全なフォールバック
 * ============================================================================
 */

#include "MyLED.h"

// プログラム全体で1個だけ存在する管理役の「実体」。
// （ヘッダでは extern で「どこかにあるよ」と宣言しただけ。実体はここに1つ置く）
LEDManager LED;

// ---- ESP32 用: ハードウェアタイマーのハンドル（AVR では使わない）----
#if defined(ESP32)
static hw_timer_t* s_hwTimer = nullptr;
#endif


/* ===========================================================================
 *  LEDClass の実装
 * =========================================================================== */

// コンストラクタ: すべての状態を「安全な初期値」にそろえておく。
// _pin = 255 は「まだ begin() されていない」ことを表す番兵（ありえないピン番号）。
LEDClass::LEDClass()
    : _pin(255), _port(nullptr), _bitmask(0),
      _toggle_interval_ms(0), _timer_counter(0),
      _toggle_enabled(false), _state(false) {}

// 使うピンを登録し、出力モードにする。
void LEDClass::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);

#if defined(__AVR__)
    // AVR では「ポートを直接叩く」ために、ピン番号から
    //   _port    … そのピンが属する出力レジスタのアドレス
    //   _bitmask … そのレジスタ内でこのピンが占めるビット
    // を求めておく。これで digitalWrite より速く点灯/消灯できる。
    _port    = portOutputRegister(digitalPinToPort(_pin));
    _bitmask = digitalPinToBitMask(_pin);
#endif
    // ESP32 等では _port は nullptr のまま → 下の setLow/HighInline() で
    // 自動的に digitalWrite が使われる。

    setLowInline(); // 最初は必ず消灯からスタート
}

// ---- ここから下の2つの free 関数は「AVRアセンブラの実演」用の裏方 ----
//      static を付けてこのファイルの中だけで使う関数にしている。
#if defined(__AVR__)
// HIGH（点灯）: ポートの該当ビットだけを 1 にする（他ビットは保持）
static inline void setHighInlineImpl(volatile uint8_t* port, uint8_t mask) {
    __asm__ __volatile__ (
        "ld  __tmp_reg__, Z    \n\t" // Z(=port) が指す現在値を一時レジスタへ読み込み
        "or  __tmp_reg__, %0   \n\t" // mask とビットOR（該当ビットを1にする）
        "st  Z, __tmp_reg__    \n\t" // ポートへ書き戻す
        :
        : "r" (mask), "z" (port)
        : "memory"
    );
}
// LOW（消灯）: ポートの該当ビットだけを 0 にする（他ビットは保持）
static inline void setLowInlineImpl(volatile uint8_t* port, uint8_t mask) {
    uint8_t inv_mask = ~mask;                // mask を反転（該当ビットだけ0、他は1）
    __asm__ __volatile__ (
        "ld  __tmp_reg__, Z    \n\t"
        "and __tmp_reg__, %0   \n\t"         // 反転maskとビットAND（該当ビットを0にする）
        "st  Z, __tmp_reg__    \n\t"
        :
        : "r" (inv_mask), "z" (port)
        : "memory"
    );
}
#endif // __AVR__

// 点灯。AVRなら高速アセンブラ、それ以外は素直に digitalWrite。
void LEDClass::setHighInline() {
#if defined(__AVR__)
    if (_port) { setHighInlineImpl(_port, _bitmask); return; }
#endif
    digitalWrite(_pin, HIGH);
}

// 消灯。同上。
void LEDClass::setLowInline() {
#if defined(__AVR__)
    if (_port) { setLowInlineImpl(_port, _bitmask); return; }
#endif
    digitalWrite(_pin, LOW);
}

// t ミリ秒ごとの点滅を設定する。t<=0 なら点滅停止。
void LEDClass::toggle(int t) {
    // これらの変数はタイマー割り込み(_tick1ms)からも読み書きされる。
    // 複数の変数を書き換える「途中」で割り込みが入るとズレることがあるため、
    // 書き換えのあいだだけ割り込みを一瞬止めて安全に更新する（アトミック更新）。
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

// PWM（明るさ調整）。0=消灯〜255=全灯。
void LEDClass::pwm(uint8_t duty) {
    _toggle_enabled = false; // PWM中は点滅トグルを止める
#if defined(__AVR__)
    // 【注意 / AVRのみ】このライブラリは Timer1 を点滅用に占有するため、
    // Timer1 につながる 9・10 番ピンの analogWrite は正しく動きません。
    // PWM を使うピンは 3・5・6・11 番（Timer0/Timer2）を選んでください。
#endif
    analogWrite(_pin, duty);
}

// 1ミリ秒ごとにタイマー割り込みから呼ばれる。ここが点滅の心臓部。
void LEDClass::_tick1ms() {
    if (!_toggle_enabled || _toggle_interval_ms == 0) return;

    _timer_counter++;
    if (_timer_counter >= _toggle_interval_ms) {
        _timer_counter = 0;
        _state = !_state;                 // 点↔消をひっくり返す
        if (_state) setHighInline();
        else        setLowInline();
    }
}


/* ===========================================================================
 *  LEDManager の実装
 * =========================================================================== */

LEDManager::LEDManager() : _leds(nullptr), _count(0) {}

// 管理する LED 配列を登録し、1msタイマーを起動する。
void LEDManager::begin(LEDClass* ledArray, size_t count) {
    _leds  = ledArray;
    _count = count;
    initTimer1();
}

// LED[n] を可能にする添字演算子。範囲外や未初期化のときは 0番を返して事故防止。
LEDClass& LEDManager::operator[](size_t index) {
    if (_leds == nullptr) {
        // begin() 前に呼ばれた場合の保険。静的なダミーを返して落ちないようにする。
        static LEDClass dummy;
        return dummy;
    }
    if (index >= _count) index = 0; // 範囲外アクセス保護
    return _leds[index];
}

// 全 LED の 1ms 処理をまとめて実行（割り込みから呼ばれる）。
void LEDManager::_tickAll() {
    for (size_t i = 0; i < _count; i++) {
        _leds[i]._tick1ms();
    }
}


/* ===========================================================================
 *  1ms タイマー割り込みのセットアップ（ここがマイコン依存の核心）
 * =========================================================================== */

#if defined(__AVR__)
// ---- AVR（UNO/Nano/LGT8F）: Timer1 を CTC モードで 1kHz(=1ms) に設定 ----
void LEDManager::initTimer1() {
    noInterrupts();          // 設定中は割り込みを止める
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    // 比較値 OCR1A を F_CPU から自動計算（16MHzなら 249 になる）。
    //   割り込み周波数 = F_CPU / プリスケーラ(64) / (OCR1A + 1) = 1000Hz
    OCR1A  = (uint16_t)(F_CPU / 64UL / 1000UL - 1UL);

    TCCR1B |= (1 << WGM12);              // CTC モード（OCR1A で自動リセット）
    TCCR1B |= (1 << CS11) | (1 << CS10); // プリスケーラ 64
    TIMSK1 |= (1 << OCIE1A);             // 比較一致A割り込みを許可
    interrupts();
}

// AVR の割り込みサービスルーチン。1ms ごとに自動で呼ばれる。
ISR(TIMER1_COMPA_vect) {
    LED._tickAll();
}

#elif defined(ESP32)
// ---- ESP32 / C3 / S2 / S3: 汎用ハードウェアタイマーで 1ms 周期の割り込み ----
// ※ 割り込み処理は RAM 上に置く必要があるため IRAM_ATTR を付ける。
void IRAM_ATTR onLedTimer() {
    LED._tickAll();
}

void LEDManager::initTimer1() {
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // arduino-esp32 3.x 以降の新API。1MHz(=1μs刻み)のタイマーを作る。
    s_hwTimer = timerBegin(1000000);              // 分解能 1MHz
    timerAttachInterrupt(s_hwTimer, &onLedTimer); // 割り込み関数を登録
    timerAlarm(s_hwTimer, 1000, true, 0);         // 1000μs=1ms ごと・自動再読込
  #else
    // arduino-esp32 2.x 以前の旧API（互換のため残す）。
    s_hwTimer = timerBegin(0, 80, true);          // 80MHz/80 = 1MHz
    timerAttachInterrupt(s_hwTimer, &onLedTimer, true);
    timerAlarmWrite(s_hwTimer, 1000, true);       // 1ms ごと
    timerAlarmEnable(s_hwTimer);
  #endif
}

#else
// ---- 上記以外のマイコン: タイマー無しのフォールバック ----
// （タイマー割り込みが用意できない環境向け。点滅は動きませんが、
//   setHighInline()/setLowInline()/pwm() は使えます。）
void LEDManager::initTimer1() {
    // 何もしない
}
#endif
