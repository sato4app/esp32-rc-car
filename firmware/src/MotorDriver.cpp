#include "MotorDriver.h"
#include "config.h"

static const uint32_t MAX_DUTY = (1u << MOTOR_PWM_BITS) - 1;

// Arduino-ESP32 のコア3.xでLEDC(PWM)の関数が変わったため、両方で動くようにする
static void pwmAttach(uint8_t pin, uint8_t channel) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(pin, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS, channel);
#else
  ledcSetup(channel, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS);
  ledcAttachPin(pin, channel);
#endif
}

static void pwmWrite(uint8_t pin, uint8_t channel, uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(channel, duty);
#endif
}

void MotorDriver::begin() {
  left_  = {PIN_AIN1, PIN_AIN2, PIN_PWMA, 0, LEFT_MOTOR_INVERT};
  right_ = {PIN_BIN1, PIN_BIN2, PIN_PWMB, 1, RIGHT_MOTOR_INVERT};

  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, LOW); // 接続されるまでは切り離しておく

  for (const Channel *ch : {&left_, &right_}) {
    pinMode(ch->in1, OUTPUT);
    pinMode(ch->in2, OUTPUT);
    pwmAttach(ch->pwmPin, ch->ledcChannel);
  }
  brake();
}

void MotorDriver::setEnabled(bool enabled) {
  if (!enabled) brake();
  digitalWrite(PIN_STBY, enabled ? HIGH : LOW);
}

void MotorDriver::set(int leftPercent, int rightPercent) {
  drive(left_, leftPercent);
  drive(right_, rightPercent);
}

void MotorDriver::brake() {
  drive(left_, 0);
  drive(right_, 0);
}

// TB6612FNGの真理値表
//   IN1=H IN2=L → 正転 / IN1=L IN2=H → 逆転 / IN1=H IN2=H → ショートブレーキ
void MotorDriver::drive(const Channel &ch, int percent) {
  percent = constrain(percent, -100, 100);
  if (ch.invert) percent = -percent;

  if (percent == 0) {
    digitalWrite(ch.in1, HIGH);
    digitalWrite(ch.in2, HIGH);
    pwmWrite(ch.pwmPin, ch.ledcChannel, 0);
    return;
  }

  digitalWrite(ch.in1, percent > 0 ? HIGH : LOW);
  digitalWrite(ch.in2, percent > 0 ? LOW : HIGH);
  // 100%入力のときに MOTOR_MAX_DUTY_PERCENT になるよう縮める
  uint32_t duty = (uint32_t)abs(percent) * MAX_DUTY * MOTOR_MAX_DUTY_PERCENT / 10000;
  pwmWrite(ch.pwmPin, ch.ledcChannel, duty);
}
