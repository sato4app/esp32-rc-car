#include "DriveController.h"
#include "config.h"

void DriveController::command(int throttle, int steer, bool emergencyStop) {
  commandFor(throttle, steer, FAILSAFE_TIMEOUT_MS);
  emergencyStop_ = emergencyStop;
}

void DriveController::commandFor(int throttle, int steer, uint32_t durationMs) {
  throttle = constrain(throttle, -100, 100);
  steer = constrain(steer, -100, 100) * STEER_GAIN_PERCENT / 100;

  int left = throttle + steer;
  int right = throttle - steer;
  // どちらかが100を超えたら、左右の比率を保ったまま縮める（曲がり具合を変えない）
  int peak = max(abs(left), abs(right));
  if (peak > 100) {
    left = left * 100 / peak;
    right = right * 100 / peak;
  }

  leftTarget_ = left;
  rightTarget_ = right;
  emergencyStop_ = false;
  lastCommandMs_ = millis();
  timeoutMs_ = durationMs;
}

void DriveController::stop() {
  leftTarget_ = rightTarget_ = 0;
  leftOut_ = rightOut_ = 0;
  motor_.brake();
}

void DriveController::setInhibited(bool inhibited) {
  inhibited_ = inhibited;
  if (inhibited) stop();
}

void DriveController::update() {
  uint32_t now = millis();
  float dtSec = (now - lastUpdateMs_) / 1000.0f;
  lastUpdateMs_ = now;

  failsafe_ = (now - lastCommandMs_) > timeoutMs_;

  // 非常停止・通信途絶・走行禁止は、加速制限をかけずに即座に止める
  if (failsafe_ || emergencyStop_ || inhibited_) {
    if (leftOut_ != 0 || rightOut_ != 0) stop();
    return;
  }

  float maxStep = RAMP_PERCENT_PER_SEC * dtSec;
  leftOut_ = approach(leftOut_, leftTarget_, maxStep);
  rightOut_ = approach(rightOut_, rightTarget_, maxStep);
  motor_.set((int)leftOut_, (int)rightOut_);
}

float DriveController::approach(float current, float target, float maxStep) {
  if (current < target) return min(current + maxStep, target);
  if (current > target) return max(current - maxStep, target);
  return current;
}
