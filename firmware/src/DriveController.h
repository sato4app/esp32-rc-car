#pragma once
#include <Arduino.h>
#include "MotorDriver.h"

// スマホからの「前後・旋回」を左右モーターの出力に変換し、安全のための制限をかける。
//   - 左右配分（戦車式）: 左 = 前後 + 旋回、右 = 前後 - 旋回
//   - 加速制限: 出力を少しずつ変えて突入電流を抑える
//   - フェイルセーフ: 操作データが途絶えたら即停止する
class DriveController {
public:
  explicit DriveController(MotorDriver &motor) : motor_(motor) {}

  // 操作データを受け取る（-100〜100）。受け取った時刻をフェイルセーフに使う
  void command(int throttle, int steer, bool emergencyStop);
  // 一定時間だけ走らせる（シリアルからの動作確認用）
  void commandFor(int throttle, int steer, uint32_t durationMs);
  void stop();                     // 即停止（切断・低電圧など）
  void setInhibited(bool inhibited); // trueの間は操作を受け付けず停止し続ける
  void update();                   // 制御ループから一定周期で呼ぶ

  int leftOutput() const  { return (int)leftOut_; }
  int rightOutput() const { return (int)rightOut_; }
  bool failsafeActive() const { return failsafe_; }
  bool emergencyStopped() const { return emergencyStop_; }

private:
  static float approach(float current, float target, float maxStep);

  MotorDriver &motor_;
  int leftTarget_ = 0;
  int rightTarget_ = 0;
  float leftOut_ = 0;
  float rightOut_ = 0;
  uint32_t lastCommandMs_ = 0;
  uint32_t timeoutMs_ = 0;
  uint32_t lastUpdateMs_ = 0;
  bool failsafe_ = true; // 最初の操作データが届くまでは停止
  bool emergencyStop_ = false;
  bool inhibited_ = false;
};
