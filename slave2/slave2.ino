#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Hardware Pin Configuration
const uint8_t LED_PIN = 4;

// Slave 2 BLE Identifiers
#define SERVICE_UUID        "2b92513e-afed-40ac-afa6-e54c63acdcec"
#define CHARACTERISTIC_UUID "ea3d7c32-0013-4447-8f7b-47b959b5cddc"

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("[Slave 2] Master connected.");
  }

  void onDisconnect(BLEServer *pServer) {
    Serial.println("[Slave 2] Master disconnected. Resuming advertising...");
    BLEDevice::startAdvertising();
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value.length() > 0) {
      char cmd = value[0];
      Serial.printf("[Slave 2] Received Command: %c -> ", cmd);
      if (cmd == '1') {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("LED turned ON");
      } else {
        digitalWrite(LED_PIN, LOW);
        Serial.println("LED turned OFF");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  BLEDevice::init("BCA188-Slave-2");
  Serial.print("[Slave 2] Address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("[Slave 2] Service active and advertising.");
}

void loop() {
  delay(1000);
}