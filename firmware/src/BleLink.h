#pragma once
#include <Arduino.h>

// スマホから届く操作データ（Control キャラクタリスティック、3バイト）
//   [0] throttle  int8  -100〜100（前進が正）
//   [1] steer     int8  -100〜100（右旋回が正）
//   [2] flags     uint8 bit0: 非常停止
struct ControlPacket {
  int8_t throttle;
  int8_t steer;
  uint8_t flags;
};
#define CONTROL_FLAG_EMERGENCY_STOP 0x01

// スマホへ送る状態（Telemetry キャラクタリスティック、5バイト、Notify）
//   [0-1] battery_mv uint16 リトルエンディアン（電池なしは0）
//   [2]   state      uint8  bit0: 低電圧で停止中 / bit1: 通信途絶で停止中 / bit2: 非常停止中
//   [3]   left       int8   左モーターの出力 -100〜100
//   [4]   right      int8   右モーターの出力 -100〜100
struct TelemetryPacket {
  uint16_t batteryMv;
  uint8_t state;
  int8_t left;
  int8_t right;
} __attribute__((packed));
#define STATE_LOW_BATTERY    0x01
#define STATE_FAILSAFE       0x02
#define STATE_EMERGENCY_STOP 0x04

// NimBLEのGATTサーバ。BLEのコールバックは別タスクで動くため、
// 受信データはキューを通してloop()側で取り出す。
namespace BleLink {
  void begin();
  bool connected();
  // 接続・切断が起きたら true を返し、新しい状態を connected に入れる
  bool pollConnectionChange(bool &connected);
  // 新しい操作データがあれば true（最新の1件だけを保持する）
  bool pollControl(ControlPacket &packet);
  void notifyTelemetry(const TelemetryPacket &packet);
}
