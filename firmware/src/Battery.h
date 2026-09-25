#pragma once
#include <Arduino.h>
#include "config.h"

// 分圧抵抗を通して2S電池の電圧を測り、低電圧を判定する。
// 一度「低電圧」になったら、電源を入れ直すまで解除しない
// （止めると負荷が減って電圧が戻り、走る・止まるを繰り返すため）。
class Battery {
public:
  void begin();
  void update(); // 制御ループから一定周期で呼ぶ

  // 電池がつながっていなければ0（USB給電で開発中）
  uint16_t millivolts() const { return present() ? (uint16_t)mv_ : 0; }
  bool present() const { return mv_ >= VBAT_PRESENT_MV; }
  bool low() const { return low_; }

private:
  float mv_ = 0; // 平滑化した電圧
  bool low_ = false;
  bool below_ = false;
  uint32_t belowSinceMs_ = 0;
};
