#pragma once
#include <Arduino.h>

// TB6612FNG で左右2つのDCモーターを回す。
// 出力は -100〜100(%)。正で前進、負で後退、0でブレーキ。
class MotorDriver {
public:
  void begin();
  void setEnabled(bool enabled); // falseでSTBYをLOWにし、両モーターを切り離す
  void set(int leftPercent, int rightPercent);
  void brake();                  // 両モーターを即座に止める（ショートブレーキ）

private:
  struct Channel {
    uint8_t in1, in2, pwmPin, ledcChannel;
    bool invert;
  };
  void drive(const Channel &ch, int percent);

  Channel left_  = {};
  Channel right_ = {};
};
