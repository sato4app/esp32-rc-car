#include "BleLink.h"
#include <NimBLEDevice.h>
#include "config.h"

namespace {
  NimBLEServer *server = nullptr;
  NimBLECharacteristic *telemetryChar = nullptr;
  QueueHandle_t controlQueue;    // 長さ1。常に最新の操作データで上書きする
  QueueHandle_t connectionQueue; // 接続(true)・切断(false)の出来事

  class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
      // 操作の遅れを減らすため、接続間隔を短く(7.5〜15ms)するよう要求する。
      // Web Bluetooth側からは指定できないので、ESP32側から頼む。
      // 引数: 最小・最大間隔(1.25ms単位), スレーブレイテンシ, 監視タイムアウト(10ms単位)
      pServer->updateConnParams(connInfo.getConnHandle(), 6, 12, 0, 200);
      bool connected = true;
      xQueueSend(connectionQueue, &connected, 0);
    }
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override {
      bool connected = false;
      xQueueSend(connectionQueue, &connected, 0);
    }
  };

  class ControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
      NimBLEAttValue value = pCharacteristic->getValue();
      if (value.size() < sizeof(ControlPacket)) return; // 形式の違うデータは捨てる
      ControlPacket packet;
      memcpy(&packet, value.data(), sizeof(packet));
      xQueueOverwrite(controlQueue, &packet);
    }
  };
}

void BleLink::begin() {
  controlQueue = xQueueCreate(1, sizeof(ControlPacket));
  connectionQueue = xQueueCreate(4, sizeof(bool));

  NimBLEDevice::init(DEVICE_NAME);
  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  server->advertiseOnDisconnect(true); // 切断後に再検索可能にする

  NimBLEService *service = server->createService(SERVICE_UUID);

  // 応答なし書き込み(WRITE_NR)を許可し、20Hzで送っても待ちが発生しないようにする
  NimBLECharacteristic *controlChar = service->createCharacteristic(
      CONTROL_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  controlChar->setCallbacks(new ControlCallbacks());

  telemetryChar = service->createCharacteristic(
      TELEMETRY_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  service->start();

  // 128bitのUUIDと名前は広告パケット(31バイト)に両方は入らないので、名前だけ載せる。
  // Web側は名前で絞り込み、サービスは optionalServices で指定する。
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setName(DEVICE_NAME);
  advertising->start();
}

bool BleLink::connected() {
  return server && server->getConnectedCount() > 0;
}

bool BleLink::pollConnectionChange(bool &connected) {
  return xQueueReceive(connectionQueue, &connected, 0) == pdTRUE;
}

bool BleLink::pollControl(ControlPacket &packet) {
  return xQueueReceive(controlQueue, &packet, 0) == pdTRUE;
}

void BleLink::notifyTelemetry(const TelemetryPacket &packet) {
  if (!connected()) return;
  telemetryChar->setValue((const uint8_t *)&packet, sizeof(packet));
  telemetryChar->notify();
}
