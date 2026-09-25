#pragma once

// ===== ピン割り当て（ESP32-C3） =====
// GPIO2/8/9 は起動モードを決めるピン、GPIO18/19 はUSBなので避けている。
// TB6612FNG のA側を左モーター、B側を右モーターにつなぐ。
#define PIN_AIN1  4
#define PIN_AIN2  5
#define PIN_PWMA  6
#define PIN_BIN1  0
#define PIN_BIN2  1
#define PIN_PWMB  7
#define PIN_STBY  10 // LOWで両モーターを切り離す（3.3Vに直結しない）
#define PIN_VBAT  3  // 電池電圧の分圧入力（ADC1）

#define LED_PIN 8 // 内蔵LEDのGPIO番号
// ESP32-C3 Super Miniなど、GPIO8のLEDはLOWで点灯する基板が多い。
// 待機中にLEDが点きっぱなしになる場合は HIGH と LOW を入れ替える。
#define LED_ON_LEVEL  LOW
#define LED_OFF_LEVEL HIGH

// ===== BLE =====
#define DEVICE_NAME "ESP32-RC-Car" // スマホに表示される名前（index.htmlと合わせる）

// 独自サービスのUUID（index.htmlと合わせる）
#define SERVICE_UUID           "8f1e0001-6c1b-4f5a-9d3e-2a7c5b4e1d00"
#define CONTROL_CHAR_UUID      "8f1e0002-6c1b-4f5a-9d3e-2a7c5b4e1d00" // Web → ESP32
#define TELEMETRY_CHAR_UUID    "8f1e0003-6c1b-4f5a-9d3e-2a7c5b4e1d00" // ESP32 → Web

// ===== モーター =====
#define MOTOR_PWM_FREQ_HZ 20000 // 可聴域より上にしてモーターの「キーン」音を消す
#define MOTOR_PWM_BITS    10    // 0〜1023

// TTモーターの定格は3〜6V。2S(満充電8.4V)をそのまま加えないよう、PWMの上限を絞る。
// 8.4V × 70% ≒ 5.9V
#define MOTOR_MAX_DUTY_PERCENT 70

// 配線の都合で前進が逆回転になるモーターは true にする
#define LEFT_MOTOR_INVERT  false
#define RIGHT_MOTOR_INVERT false

// ===== 走行制御 =====
#define CONTROL_INTERVAL_MS  10  // 制御ループの周期（100Hz）
#define FAILSAFE_TIMEOUT_MS  300 // この時間、操作データが届かなければ停止する
#define RAMP_PERCENT_PER_SEC 400 // 出力の変化速度（0→100%を0.25秒）。突入電流を抑える
#define STEER_GAIN_PERCENT   60  // その場旋回が速すぎないよう、旋回量を絞る

// ===== 電池（2S Li-ion） =====
// 分圧: 電池+ ─ 100kΩ ─ PIN_VBAT ─ 33kΩ ─ GND （8.4V → 約2.08V）
// ESP32-C3のADCは約2.5Vまでしか正しく測れないため、47kΩではなく33kΩにしている
#define VBAT_R_TOP_KOHM    100
#define VBAT_R_BOTTOM_KOHM 33
#define VBAT_LOW_MV        6400 // これを下回り続けたら走行を止める（1セル3.2V）
#define VBAT_LOW_HOLD_MS   3000 // 加速時の一瞬の電圧低下では止めない
#define VBAT_PRESENT_MV    3000 // これ未満は「電池なし」（USB給電で開発中）とみなす

#define TELEMETRY_INTERVAL_MS 250 // スマホへ状態を送る周期
