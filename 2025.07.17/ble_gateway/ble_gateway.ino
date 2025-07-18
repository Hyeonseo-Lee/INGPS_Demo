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

// ====== 객체 선언 ======
HardwareSerial MySerial(1);
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLEScan* pBLEScan = nullptr;

// ====== 상태 변수 ======
bool doConnect = false;
bool connected = false;
bool doScan = false;
String targetDeviceName = "SensorNode_1";
BLEAdvertisedDevice* myDevice = nullptr;

// ====== 데이터 버퍼링 ======
String dataBuffer = "";
unsigned long lastDataTime = 0;
const unsigned long DATA_TIMEOUT = 2000;  // 2초 후 버퍼 처리 (더 길게)

// ====== 연결 상태 저장 ======
unsigned long lastConnectionAttempt = 0;
unsigned long lastScanTime = 0;
const unsigned long CONNECTION_RETRY_INTERVAL = 5000;  // 5초
const unsigned long SCAN_INTERVAL = 10000;             // 10초

// ====== JSON 처리 함수 ======
void processCompleteJson(String jsonData) {
  MySerial.println("📨 완전한 JSON 데이터: " + jsonData);
  
  // JSON 파싱
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    MySerial.println("❌ JSON 파싱 오류: " + String(error.c_str()));
    return;
  }
  
  // 데이터 추출 및 처리 (수정된 키 이름 사용)
  String macAddress = doc["mac"].as<String>();
  
  if (doc.containsKey("error")) {
    String errorMsg = doc["error"];
    MySerial.println("⚠️ 센서 오류 - MAC: " + macAddress + ", 오류: " + errorMsg);
  } else if (doc.containsKey("temp")) {
    float temperature = doc["temp"];
    MySerial.printf("🌡️ 온도 데이터 - MAC: %s, 온도: %.2f°C\n", 
                    macAddress.c_str(), temperature);
    
    // 여기에 추가 처리 로직 추가 가능
    // 예: 온도 임계값 체크, 데이터 저장 등
    if (temperature > 30.0) {
      MySerial.println("🔥 고온 경고! 온도가 30°C를 초과했습니다.");
    }
  }
}

// ====== 알림 콜백 함수 ======
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  
  String receivedChunk = String((char*)pData, length);
  MySerial.println("📤 수신된 청크: " + receivedChunk);
  
  // 버퍼에 데이터 추가
  dataBuffer += receivedChunk;
  lastDataTime = millis();
  
  // JSON 완료 체크 ('}' 문자로 종료 확인)
  if (dataBuffer.endsWith("}")) {
    processCompleteJson(dataBuffer);
    dataBuffer = "";  // 버퍼 초기화
  }
}

// ====== 클라이언트 연결 콜백 ======
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    MySerial.println("✅ BLE 서버에 연결됨!");
    connected = true;
  }

  void onDisconnect(BLEClient* pclient) override {
    MySerial.println("❌ BLE 서버에서 연결 해제됨");
    connected = false;
    doScan = true;  // 재스캔 시작
  }
};

// ====== 장치 스캔 콜백 ======
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    MySerial.printf("📡 BLE 장치 발견: %s\n", advertisedDevice.toString().c_str());
    
    // 찾고자 하는 장치 이름 확인
    if (advertisedDevice.haveName() && 
        advertisedDevice.getName() == targetDeviceName) {
      MySerial.println("🎯 타겟 장치 발견: " + targetDeviceName);
      
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    }
  }
};

// ====== 서버 연결 함수 ======
bool connectToServer() {
  MySerial.printf("🔗 서버 연결 시도: %s\n", myDevice->getAddress().toString().c_str());
  
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  // 서버 연결
  if (!pClient->connect(myDevice)) {
    MySerial.println("❌ 서버 연결 실패");
    return false;
  }
  
  MySerial.println("✅ 서버 연결 성공");
  
  // MTU 크기 요청
  pClient->setMTU(512);
  MySerial.println("📡 MTU 크기 증가 요청");
  
  // 서비스 참조 획득
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    MySerial.println("❌ 서비스를 찾을 수 없음");
    pClient->disconnect();
    return false;
  }
  
  MySerial.println("✅ 서비스 발견됨");
  
  // 특성 참조 획득
  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    MySerial.println("❌ 특성을 찾을 수 없음");
    pClient->disconnect();
    return false;
  }
  
  MySerial.println("✅ 특성 발견됨");
  
  // 알림 등록
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    MySerial.println("✅ 알림 등록 완료");
  } else {
    MySerial.println("❌ 알림 기능 지원 안됨");
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

void setup() {
  // UART1 시작
  MySerial.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(1000);
  MySerial.println("=== BLE 수신기 시작 ===");
  MySerial.println("✅ UART1 시작됨");
  
  // BLE 초기화
  BLEDevice::init("BLE_Receiver");
  
  // MTU 크기 설정
  BLEDevice::setMTU(512);
  
  MySerial.println("✅ BLE 초기화 완료 (MTU: 512)");
  
  // 스캔 시작
  doScan = true;
  MySerial.println("🔍 타겟 장치 검색 중: " + targetDeviceName);
}

void loop() {
  // 연결 시도
  if (doConnect && !connected) {
    if (connectToServer()) {
      MySerial.println("🎉 연결 및 설정 완료!");
    } else {
      MySerial.println("❌ 연결 실패, 재시도 예정");
      lastConnectionAttempt = millis();
    }
    doConnect = false;
  }
  
  // 스캔 시작
  if (doScan && !connected) {
    unsigned long currentTime = millis();
    if (currentTime - lastScanTime >= SCAN_INTERVAL) {
      startScan();
      lastScanTime = currentTime;
    }
  }
  
  // 연결 실패 후 재시도
  if (!connected && myDevice != nullptr) {
    unsigned long currentTime = millis();
    if (currentTime - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
      MySerial.println("🔄 연결 재시도...");
      doConnect = true;
      lastConnectionAttempt = currentTime;
    }
  }
  
  // 연결 상태 확인
  if (connected && pClient != nullptr && !pClient->isConnected()) {
    MySerial.println("⚠️ 연결 상태 불일치 감지, 재설정");
    connected = false;
    doScan = true;
  }
  
  // 버퍼 타임아웃 처리
  if (dataBuffer.length() > 0) {
    unsigned long currentTime = millis();
    if (currentTime - lastDataTime >= DATA_TIMEOUT) {
      MySerial.println("⏰ 데이터 수신 타임아웃, 버퍼 초기화");
      MySerial.println("🗑️ 버려진 데이터: " + dataBuffer);
      dataBuffer = "";
    }
  }
  
  delay(100);  // 100ms 대기 (더 빠른 응답을 위해 단축)
}