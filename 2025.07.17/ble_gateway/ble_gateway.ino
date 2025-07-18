#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

// ====== 핀 설정 ======
#define UART_RX 44
#define UART_TX 43

// ====== UUID (송신 코드와 동일) ======
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-90ab-cdef-1234567890ab"

// ====== 센서 노드 구조체 ======
struct SensorNode {
  String name;
  BLEClient* pClient;
  BLERemoteCharacteristic* pRemoteCharacteristic;
  BLEAdvertisedDevice* device;
  bool connected;
  bool doConnect;
  String dataBuffer;
  unsigned long lastDataTime;
  unsigned long lastConnectionAttempt;
};

// ====== 객체 선언 ======
HardwareSerial MySerial(1);
BLEScan* pBLEScan = nullptr;

// ====== 센서 노드 배열 ======
const int NUM_SENSORS = 2;
SensorNode sensors[NUM_SENSORS];

// ====== 상태 변수 ======
bool doScan = false;
unsigned long lastScanTime = 0;
const unsigned long DATA_TIMEOUT = 2000;  // 2초 후 버퍼 처리
const unsigned long CONNECTION_RETRY_INTERVAL = 5000;  // 5초
const unsigned long SCAN_INTERVAL = 10000;             // 10초

// ====== 센서 노드 초기화 ======
void initializeSensors() {
  sensors[0].name = "SensorNode_1";
  sensors[1].name = "SensorNode_2";
  
  for (int i = 0; i < NUM_SENSORS; i++) {
    sensors[i].pClient = nullptr;
    sensors[i].pRemoteCharacteristic = nullptr;
    sensors[i].device = nullptr;
    sensors[i].connected = false;
    sensors[i].doConnect = false;
    sensors[i].dataBuffer = "";
    sensors[i].lastDataTime = 0;
    sensors[i].lastConnectionAttempt = 0;
  }
}

// ====== JSON 처리 함수 ======
void processCompleteJson(String jsonData, int sensorIndex) {
  MySerial.println("📨 [" + sensors[sensorIndex].name + "] 완전한 JSON 데이터: " + jsonData);
  
  // JSON 파싱
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] JSON 파싱 오류: " + String(error.c_str()));
    return;
  }
  
  // 데이터 추출 및 처리
  String macAddress = doc["mac"].as<String>();
  
  if (doc.containsKey("error")) {
    String errorMsg = doc["error"];
    MySerial.println("⚠️ [" + sensors[sensorIndex].name + "] 센서 오류 - MAC: " + macAddress + ", 오류: " + errorMsg);
  } else if (doc.containsKey("temp")) {
    float temperature = doc["temp"];
    MySerial.printf("🌡️ [%s] 온도 데이터 - MAC: %s, 온도: %.2f°C\n", 
                    sensors[sensorIndex].name.c_str(), macAddress.c_str(), temperature);
    
    // 온도 임계값 체크
    if (temperature > 30.0) {
      MySerial.println("🔥 [" + sensors[sensorIndex].name + "] 고온 경고! 온도가 30°C를 초과했습니다.");
    }
  }
}

// ====== 알림 콜백 함수 (센서별) ======
class SensorNotifyCallback : public BLEClientCallbacks {
private:
  int sensorIndex;
  
public:
  SensorNotifyCallback(int index) : sensorIndex(index) {}
  
  void onConnect(BLEClient* pclient) override {
    MySerial.println("✅ [" + sensors[sensorIndex].name + "] BLE 서버에 연결됨!");
    sensors[sensorIndex].connected = true;
  }

  void onDisconnect(BLEClient* pclient) override {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] BLE 서버에서 연결 해제됨");
    sensors[sensorIndex].connected = false;
    doScan = true;  // 재스캔 시작
  }
};

// ====== 알림 콜백 함수들 ======
void notifyCallback1(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  String receivedChunk = String((char*)pData, length);
  MySerial.println("📤 [SensorNode_1] 수신된 청크: " + receivedChunk);
  
  sensors[0].dataBuffer += receivedChunk;
  sensors[0].lastDataTime = millis();
  
  if (sensors[0].dataBuffer.endsWith("}")) {
    processCompleteJson(sensors[0].dataBuffer, 0);
    sensors[0].dataBuffer = "";
  }
}

void notifyCallback2(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  String receivedChunk = String((char*)pData, length);
  MySerial.println("📤 [SensorNode_2] 수신된 청크: " + receivedChunk);
  
  sensors[1].dataBuffer += receivedChunk;
  sensors[1].lastDataTime = millis();
  
  if (sensors[1].dataBuffer.endsWith("}")) {
    processCompleteJson(sensors[1].dataBuffer, 1);
    sensors[1].dataBuffer = "";
  }
}

// ====== 장치 스캔 콜백 ======
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    MySerial.printf("📡 BLE 장치 발견: %s\n", advertisedDevice.toString().c_str());
    
    // 각 센서 노드 확인
    if (advertisedDevice.haveName()) {
      for (int i = 0; i < NUM_SENSORS; i++) {
        if (advertisedDevice.getName() == sensors[i].name && 
            !sensors[i].connected && 
            sensors[i].device == nullptr) {
          
          MySerial.println("🎯 타겟 장치 발견: " + sensors[i].name);
          sensors[i].device = new BLEAdvertisedDevice(advertisedDevice);
          sensors[i].doConnect = true;
          break;
        }
      }
    }
  }
};

// ====== 서버 연결 함수 ======
bool connectToServer(int sensorIndex) {
  MySerial.printf("🔗 [%s] 서버 연결 시도: %s\n", 
                  sensors[sensorIndex].name.c_str(), 
                  sensors[sensorIndex].device->getAddress().toString().c_str());
  
  sensors[sensorIndex].pClient = BLEDevice::createClient();
  sensors[sensorIndex].pClient->setClientCallbacks(new SensorNotifyCallback(sensorIndex));
  
  // 서버 연결
  if (!sensors[sensorIndex].pClient->connect(sensors[sensorIndex].device)) {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] 서버 연결 실패");
    return false;
  }
  
  MySerial.println("✅ [" + sensors[sensorIndex].name + "] 서버 연결 성공");
  
  // MTU 크기 요청
  sensors[sensorIndex].pClient->setMTU(512);
  MySerial.println("📡 [" + sensors[sensorIndex].name + "] MTU 크기 증가 요청");
  
  // 서비스 참조 획득
  BLERemoteService* pRemoteService = sensors[sensorIndex].pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] 서비스를 찾을 수 없음");
    sensors[sensorIndex].pClient->disconnect();
    return false;
  }
  
  MySerial.println("✅ [" + sensors[sensorIndex].name + "] 서비스 발견됨");
  
  // 특성 참조 획득
  sensors[sensorIndex].pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (sensors[sensorIndex].pRemoteCharacteristic == nullptr) {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] 특성을 찾을 수 없음");
    sensors[sensorIndex].pClient->disconnect();
    return false;
  }
  
  MySerial.println("✅ [" + sensors[sensorIndex].name + "] 특성 발견됨");
  
  // 알림 등록 (센서별 콜백 함수 사용)
  if (sensors[sensorIndex].pRemoteCharacteristic->canNotify()) {
    if (sensorIndex == 0) {
      sensors[sensorIndex].pRemoteCharacteristic->registerForNotify(notifyCallback1);
    } else {
      sensors[sensorIndex].pRemoteCharacteristic->registerForNotify(notifyCallback2);
    }
    MySerial.println("✅ [" + sensors[sensorIndex].name + "] 알림 등록 완료");
  } else {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] 알림 기능 지원 안됨");
  }
  
  return true;
}

// ====== 스캔 시작 함수 ======
void startScan() {
  MySerial.println("🔍 BLE 장치 스캔 시작...");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);  // 5초 동안 스캔
}

// ====== 연결 상태 출력 ======
void printConnectionStatus() {
  static unsigned long lastStatusPrint = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastStatusPrint >= 15000) {  // 15초마다 상태 출력
    MySerial.println("\n=== 연결 상태 ===");
    for (int i = 0; i < NUM_SENSORS; i++) {
      String status = sensors[i].connected ? "연결됨" : "연결 안됨";
      MySerial.println(sensors[i].name + ": " + status);
    }
    MySerial.println("================\n");
    lastStatusPrint = currentTime;
  }
}

void setup() {
  // UART1 시작
  MySerial.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(1000);
  MySerial.println("=== BLE 다중 센서 수신기 시작 ===");
  MySerial.println("✅ UART1 시작됨");
  
  // 센서 노드 초기화
  initializeSensors();
  
  // BLE 초기화
  BLEDevice::init("BLE_MultiReceiver");
  
  // MTU 크기 설정
  BLEDevice::setMTU(512);
  
  MySerial.println("✅ BLE 초기화 완료 (MTU: 512)");
  
  // 스캔 시작
  doScan = true;
  MySerial.println("🔍 타겟 장치 검색 중: SensorNode_1, SensorNode_2");
}

void loop() {
  // 각 센서 노드 연결 시도
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensors[i].doConnect && !sensors[i].connected) {
      if (connectToServer(i)) {
        MySerial.println("🎉 [" + sensors[i].name + "] 연결 및 설정 완료!");
      } else {
        MySerial.println("❌ [" + sensors[i].name + "] 연결 실패, 재시도 예정");
        sensors[i].lastConnectionAttempt = millis();
      }
      sensors[i].doConnect = false;
    }
  }
  
  // 스캔 시작
  if (doScan) {
    // 모든 센서가 연결되었는지 확인
    bool allConnected = true;
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (!sensors[i].connected) {
        allConnected = false;
        break;
      }
    }
    
    if (!allConnected) {
      unsigned long currentTime = millis();
      if (currentTime - lastScanTime >= SCAN_INTERVAL) {
        startScan();
        lastScanTime = currentTime;
      }
    } else {
      doScan = false;  // 모든 센서가 연결되면 스캔 중지
    }
  }
  
  // 연결 실패 후 재시도
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (!sensors[i].connected && sensors[i].device != nullptr) {
      unsigned long currentTime = millis();
      if (currentTime - sensors[i].lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
        MySerial.println("🔄 [" + sensors[i].name + "] 연결 재시도...");
        sensors[i].doConnect = true;
        sensors[i].lastConnectionAttempt = currentTime;
      }
    }
  }
  
  // 연결 상태 확인
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensors[i].connected && sensors[i].pClient != nullptr && !sensors[i].pClient->isConnected()) {
      MySerial.println("⚠️ [" + sensors[i].name + "] 연결 상태 불일치 감지, 재설정");
      sensors[i].connected = false;
      doScan = true;
    }
  }
  
  // 버퍼 타임아웃 처리
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensors[i].dataBuffer.length() > 0) {
      unsigned long currentTime = millis();
      if (currentTime - sensors[i].lastDataTime >= DATA_TIMEOUT) {
        MySerial.println("⏰ [" + sensors[i].name + "] 데이터 수신 타임아웃, 버퍼 초기화");
        MySerial.println("🗑️ [" + sensors[i].name + "] 버려진 데이터: " + sensors[i].dataBuffer);
        sensors[i].dataBuffer = "";
      }
    }
  }
  
  // 연결 상태 출력
  printConnectionStatus();
  
  delay(100);  // 100ms 대기
}