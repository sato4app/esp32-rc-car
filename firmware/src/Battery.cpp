#include "Battery.h"

static uint16_t readBatteryMillivolts() {
  // analogReadMilliVolts はチップ個体ごとの補正値を使って電圧を返す
  uint32_t adcMv = analogReadMilliVolts(PIN_VBAT);
  return adcMv * (VBAT_R_TOP_KOHM + VBAT_R_BOTTOM_KOHM) / VBAT_R_BOTTOM_KOHM;
}

void Battery::begin() {
  pinMode(PIN_VBAT, INPUT);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);
  mv_ = readBatteryMillivolts();
}

void Battery::update() {
  // モーターのノイズで値が揺れるため、指数移動平均でならす
  mv_ += (readBatteryMillivolts() - mv_) * 0.05f;

  if (!present() || low_) return;

  uint32_t now = millis();
  if (mv_ < VBAT_LOW_MV) {
    if (!below_) {
      below_ = true;
      belowSinceMs_ = now;
    } else if (now - belowSinceMs_ >= VBAT_LOW_HOLD_MS) {
      low_ = true;
    }
  } else {
    below_ = false;
  }
}
