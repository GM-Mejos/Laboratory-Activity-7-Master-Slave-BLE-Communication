#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Hardware Pin Configuration
const uint8_t BUTTON_PIN = 18;

// Total Slaves to coordinate
const size_t NUM_SLAVES = 2;

// Target Service UUIDs defined on Slave 1 and Slave 2
static BLEUUID TARGET_SERVICE_UUIDS[NUM_SLAVES] = {
  BLEUUID("2b92513e-afed-40ac-afa6-e54c63acdceb"), // Slave 1 Service
  BLEUUID("2b92513e-afed-40ac-afa6-e54c63acdcec")  // Slave 2 Service
};
static BLEUUID TARGET_CHAR_UUID("ea3d7c32-0013-4447-8f7b-47b959b5cddc");

// Client and Remote Characteristic References
BLEAdvertisedDevice* targetSlaves[NUM_SLAVES] = {nullptr, nullptr};
BLEClient* bleClients[NUM_SLAVES] = {nullptr, nullptr};
BLERemoteCharacteristic* remoteChars[NUM_SLAVES] = {nullptr, nullptr};

int lastDispatchedState[NUM_SLAVES] = {-1, -1};
uint32_t lastScanTimestamp = 0;
const uint32_t SCAN_INTERVAL_MS = 4000;

// Button Debounce Tracking
bool currentButtonState = false;
bool stableButtonState = false;
uint32_t lastDebounceTime = 0;
const uint32_t DEBOUNCE_DELAY_MS = 35;

/**
 * Scan callback: Automatically detects slaves broadcasting matching Service UUIDs
 */
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    for (size_t i = 0; i < NUM_SLAVES; i++) {
      if (targetSlaves[i] == nullptr && 
          advertisedDevice.haveServiceUUID() && 
          advertisedDevice.isAdvertisingService(TARGET_SERVICE_UUIDS[i])) {
        
        Serial.printf("[BLE Master] Discovered Slave %u at %s\n", 
                      i + 1, advertisedDevice.getAddress().toString().c_str());
        targetSlaves[i] = new BLEAdvertisedDevice(advertisedDevice);
      }
    }
  }
};

/**
 * Connects to a discovered slave peripheral without requiring hardcoded MACs
 */
bool establishSlaveConnection(size_t index) {
  if (targetSlaves[index] == nullptr) return false;

  Serial.printf("[BLE Master] Connecting to Slave %u (%s)...\n", 
                index + 1, targetSlaves[index]->getAddress().toString().c_str());

  remoteChars[index] = nullptr;

  if (!bleClients[index]->connect(targetSlaves[index])) {
    Serial.printf("[BLE Master] Connection to Slave %u failed.\n", index + 1);
    delete targetSlaves[index];
    targetSlaves[index] = nullptr;
    return false;
  }

  BLERemoteService* pRemoteService = bleClients[index]->getService(TARGET_SERVICE_UUIDS[index]);
  if (pRemoteService != nullptr) {
    remoteChars[index] = pRemoteService->getCharacteristic(TARGET_CHAR_UUID);
  }

  if (remoteChars[index] != nullptr && remoteChars[index]->canWrite()) {
    lastDispatchedState[index] = -1; // Force immediate sync
    Serial.printf("[BLE Master] Connected to Slave %u successfully.\n", index + 1);
    return true;
  }

  Serial.printf("[BLE Master] Write characteristic setup failed for Slave %u.\n", index + 1);
  remoteChars[index] = nullptr;
  bleClients[index]->disconnect();
  delete targetSlaves[index];
  targetSlaves[index] = nullptr;
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  currentButtonState = (digitalRead(BUTTON_PIN) == LOW);
  stableButtonState = currentButtonState;

  // Initialize BLE Client
  BLEDevice::init("BCA188-Master-Central");
  Serial.println("[BLE Master] Initialized in Auto-Discovery Mode.");

  for (size_t i = 0; i < NUM_SLAVES; i++) {
    bleClients[i] = BLEDevice::createClient();
  }

  // Configure Scanner
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setInterval(1349);
  pScan->setWindow(449);
  pScan->setActiveScan(true);
}

void loop() {
  // Non-blocking button debounce
  bool rawReading = (digitalRead(BUTTON_PIN) == LOW);
  if (rawReading != currentButtonState) {
    currentButtonState = rawReading;
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) >= DEBOUNCE_DELAY_MS) {
    stableButtonState = currentButtonState;
  }

  // Link management & command dispatch
  for (size_t i = 0; i < NUM_SLAVES; i++) {
    if (!bleClients[i]->isConnected()) {
      if (remoteChars[i] != nullptr) {
        Serial.printf("[BLE Master] Lost connection to Slave %u.\n", i + 1);
        remoteChars[i] = nullptr;
      }

      if (targetSlaves[i] != nullptr) {
        establishSlaveConnection(i);
      }
    } else if (remoteChars[i] != nullptr && lastDispatchedState[i] != (int)stableButtonState) {
      uint8_t payload = stableButtonState ? '1' : '0';

      if (remoteChars[i]->writeValue(&payload, 1, true)) {
        lastDispatchedState[i] = (int)stableButtonState;
        Serial.printf("[BLE Master] Sent to Slave %u: LED %s\n", 
                      i + 1, stableButtonState ? "ON" : "OFF");
      } else {
        Serial.printf("[BLE Master] Write failed to Slave %u. Disconnecting.\n", i + 1);
        remoteChars[i] = nullptr;
        bleClients[i]->disconnect();
      }
    }
  }

  // Scan periodically if any slave is not yet discovered or connected
  if ((targetSlaves[0] == nullptr || targetSlaves[1] == nullptr) && 
      (millis() - lastScanTimestamp >= SCAN_INTERVAL_MS)) {
    lastScanTimestamp = millis();
    Serial.println("[BLE Master] Scanning for advertising Slaves...");
    BLEDevice::getScan()->start(2, false);
  }

  delay(5);
}