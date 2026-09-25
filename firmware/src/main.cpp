#include <Arduino.h>
#include "config.h"
#include "MotorDriver.h"
#include "DriveController.h"
#include "Battery.h"
#include "BleLink.h"

#define SERIAL_DRIVE_MS 1000 // シリアルからの走行コマンドで走らせる時間

MotorDriver motor;
DriveController drive(motor);
Battery battery;

String serialLine; // シリアルモニタから入力中のコマンド
bool motorEnabled = false;
bool lowBatteryReported = false;
uint32_t lastControlMs = 0;
uint32_t lastTelemetryMs = 0;

// 状態をLEDで表す
//   ゆっくり点滅: 接続待ち / 点灯: 接続中 / 速い点滅: 低電圧で停止中
void updateLed(uint32_t now) {
  bool on;
  if (battery.low()) {
    on = (now / 100) % 2;
  } else if (BleLink::connected()) {
    on = true;
  } else {
    on = (now / 500) % 2;
  }
  digitalWrite(LED_PIN, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

// モーターへの通電は「操作を受けている間」だけにする（STBYで切り離す）
void updateMotorEnable() {
  bool enable = !battery.low() && !drive.failsafeActive();
  if (enable == motorEnabled) return;
  motorEnabled = enable;
  motor.setEnabled(enable);
}

void sendTelemetry() {
  TelemetryPacket packet;
  packet.batteryMv = battery.millivolts();
  packet.state = (battery.low() ? STATE_LOW_BATTERY : 0)
               | (drive.failsafeActive() ? STATE_FAILSAFE : 0)
               | (drive.emergencyStopped() ? STATE_EMERGENCY_STOP : 0);
  packet.left = drive.leftOutput();
  packet.right = drive.rightOutput();
  BleLink::notifyTelemetry(packet);
}

// シリアルモニタからのコマンド（スマホなしでモーターを確認する）
//   D <前後> <旋回>  1秒間だけ走る（例: D 50 0 で前進50%、D 0 100 でその場右旋回）
//   S               すぐに止める
//   B               電池電圧を表示する
void handleSerialCommand(String command) {
  command.trim();
  command.toUpperCase();
  Serial.print("受信: ");
  Serial.println(command);

  if (command == "S") {
    drive.stop();
    drive.commandFor(0, 0, 0);
  } else if (command == "B") {
    Serial.printf("電池: %u mV%s\n", battery.millivolts(), battery.low() ? "（低電圧）" : "");
  } else if (command.startsWith("D ")) {
    int throttle = 0, steer = 0;
    if (sscanf(command.c_str() + 2, "%d %d", &throttle, &steer) < 1) {
      Serial.println("形式: D <前後 -100〜100> <旋回 -100〜100>");
      return;
    }
    drive.commandFor(throttle, steer, SERIAL_DRIVE_MS);
  } else {
    Serial.println("コマンド: D <前後> <旋回> / S / B");
  }
}

void readSerial() {
  // モニタは1文字ずつ送ってくるため、改行が来るまで貯めてから処理する
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialLine.length() > 0) handleSerialCommand(serialLine);
      serialLine = "";
    } else {
      serialLine += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF_LEVEL);
  motor.begin(); // 起動直後にモーターが動かないよう、最初に止めておく
  battery.begin();
  delay(1000); // ESP32-C3のUSBシリアル認識待ち
  Serial.println("BLE起動中...");

  BleLink::begin();
  Serial.println("準備完了！スマホからの接続を待っています。");
}

void loop() {
  uint32_t now = millis();
  readSerial();

  bool connected;
  while (BleLink::pollConnectionChange(connected)) {
    Serial.println(connected ? "スマホと接続しました！" : "スマホとの接続が切れました。");
    if (!connected) drive.stop();
  }

  if (now - lastControlMs < CONTROL_INTERVAL_MS) return;
  lastControlMs = now;

  ControlPacket packet;
  if (BleLink::pollControl(packet)) {
    drive.command(packet.throttle, packet.steer, packet.flags & CONTROL_FLAG_EMERGENCY_STOP);
  }

  battery.update();
  if (battery.low() && !lowBatteryReported) {
    lowBatteryReported = true;
    Serial.printf("低電圧（%u mV）のため走行を止めました。電池を充電してください。\n", battery.millivolts());
  }
  drive.setInhibited(battery.low());
  drive.update();
  updateMotorEnable();
  updateLed(now);

  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    sendTelemetry();
  }
}
