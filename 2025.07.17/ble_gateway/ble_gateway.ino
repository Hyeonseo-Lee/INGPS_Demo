#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <credentials.h>

// ====== 핀 설정 ======
#define UART_RX 44
#define UART_TX 43

// ====== MQTT 설정 ======
const char* mqttServerAddress = "9c500c1053df40c795c005da44aee8f0.s2.eu.hivemq.cloud";
const int mqttPort = 8883;

const char* mqttClientId = "ESP32_BLE_Gateway";

// MQTT 토픽 설정
const char* mqttTopicPrefix = "sensors/";
const char* mqttTopicStatus = "sensors/gateway/status";

// ====== Root CA 인증서 (HiveMQ Cloud) ======
const char* rootCACert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/\n" \
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
"DkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB\n" \
"AQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC\n" \
"ov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL\n" \
"wYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D\n" \
"LtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK\n" \
"4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5\n" \
"bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y\n" \
"sR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ\n" \
"Xmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4\n" \
"FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc\n" \
"SLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql\n" \
"PRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND\n" \
"TwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw\n" \
"SwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1\n" \
"c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx\n" \
"+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB\n" \
"ATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu\n" \
"b3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E\n" \
"U1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu\n" \
"MA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC\n" \
"5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW\n" \
"9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuG\n" \
"WCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9O\n" \
"he8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFC\n" \
"Dfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n" \
"-----END CERTIFICATE-----\n";

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
  int openBraces;      // 열린 중괄호 개수
  int closeBraces;     // 닫힌 중괄호 개수
  bool inString;       // 문자열 내부 여부
  bool escapeNext;     // 다음 문자가 이스케이프 문자인지
};

// ====== 객체 선언 ======
HardwareSerial MySerial(1);
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
BLEScan* pBLEScan = nullptr;

// ====== 센서 노드 배열 ======
const int NUM_SENSORS = 2;
SensorNode sensors[NUM_SENSORS];

// ====== 상태 변수 ======
bool doScan = false;
unsigned long lastScanTime = 0;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long DATA_TIMEOUT = 5000;  // 2초 후 버퍼 처리
const unsigned long CONNECTION_RETRY_INTERVAL = 5000;  // 5초
const unsigned long SCAN_INTERVAL = 10000;             // 10초
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;    // 5초

// ====== WiFi 연결 함수 ======
void connectToWiFi() {
  MySerial.println("📶 WiFi 연결 시도...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    MySerial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    MySerial.println();
    MySerial.println("✅ WiFi 연결 성공!");
    MySerial.print("📍 IP 주소: ");
    MySerial.println(WiFi.localIP());
  } else {
    MySerial.println();
    MySerial.println("❌ WiFi 연결 실패!");
  }
}

// ====== MQTT 연결 함수 ======
bool connectToMQTT() {
  if (mqttClient.connected()) {
    return true;
  }
  
  MySerial.println("🔗 MQTT 브로커 연결 시도...");
  
  // Root CA 인증서 설정 (프로덕션용 보안 연결)
  wifiClient.setCACert(rootCACert);
  
  MySerial.println("🔒 TLS 인증서 검증 활성화됨");
  
  if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
    MySerial.println("✅ MQTT 브로커 연결 성공!");
    
    // 게이트웨이 상태 발행
    mqttClient.publish(mqttTopicStatus, "online", true);
    
    return true;
  } else {
    MySerial.print("❌ MQTT 연결 실패, 오류 코드: ");
    MySerial.println(mqttClient.state());
    return false;
  }
}

// ====== MQTT 데이터 발행 함수 ======
void publishSensorData(const String& sensorName, const String& jsonData) {
  if (!mqttClient.connected()) {
    return;
  }
  
  String topic = mqttTopicPrefix + sensorName + "/data";
  
  if (mqttClient.publish(topic.c_str(), jsonData.c_str())) {
    MySerial.println("📤 [" + sensorName + "] MQTT 데이터 발행 성공");
  } else {
    MySerial.println("❌ [" + sensorName + "] MQTT 데이터 발행 실패");
  }
}

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
    sensors[i].openBraces = 0;
    sensors[i].closeBraces = 0;
    sensors[i].inString = false;
    sensors[i].escapeNext = false;
  }
}

// ====== JSON 완성도 체크 함수 ======
bool isCompleteJson(const String& jsonData, int sensorIndex) {
  SensorNode& sensor = sensors[sensorIndex];
  
  for (int i = 0; i < jsonData.length(); i++) {
    char c = jsonData.charAt(i);
    
    if (sensor.escapeNext) {
      sensor.escapeNext = false;
      continue;
    }
    
    if (c == '\\' && sensor.inString) {
      sensor.escapeNext = true;
      continue;
    }
    
    if (c == '"' && !sensor.escapeNext) {
      sensor.inString = !sensor.inString;
      continue;
    }
    
    if (!sensor.inString) {
      if (c == '{') {
        sensor.openBraces++;
      } else if (c == '}') {
        sensor.closeBraces++;
      }
    }
  }
  
  // JSON이 완성되었는지 확인
  return (sensor.openBraces > 0 && sensor.openBraces == sensor.closeBraces);
}

// ====== 버퍼 리셋 함수 ======
void resetSensorBuffer(int sensorIndex) {
  sensors[sensorIndex].dataBuffer = "";
  sensors[sensorIndex].openBraces = 0;
  sensors[sensorIndex].closeBraces = 0;
  sensors[sensorIndex].inString = false;
  sensors[sensorIndex].escapeNext = false;
}

// ====== JSON 처리 함수 ======
void processCompleteJson(String jsonData, int sensorIndex) {
  MySerial.println("📨 [" + sensors[sensorIndex].name + "] 완전한 JSON 데이터: " + jsonData);

  // JSON 파싱
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] JSON 파싱 오류: " + String(error.c_str()));
    return;
  }

  // 데이터 추출 및 처리
  String macAddress = doc["mac"].as<String>();

  if (doc["error"].is<String>()) {
    String errorMsg = doc["error"];
    MySerial.println("⚠️ [" + sensors[sensorIndex].name + "] 센서 오류 - MAC: " + macAddress + ", 오류: " + errorMsg);
  } else if (doc["temp"].is<float>()) {
    float temperature = doc["temp"];
    MySerial.printf("🌡️ [%s] 온도 데이터 - MAC: %s, 온도: %.2f°C\n", 
                    sensors[sensorIndex].name.c_str(), macAddress.c_str(), temperature);

    // 온도 임계값 체크
    if (temperature > 30.0) {
      MySerial.println("🔥 [" + sensors[sensorIndex].name + "] 고온 경고! 온도가 30°C를 초과했습니다.");
    }
  }
  
  // MQTT로 데이터 발행
  publishSensorData(sensors[sensorIndex].name, jsonData);
}

// ====== 알림 콜백 함수 (센서별) ======
class SensorNotifyCallback : public BLEClientCallbacks {
private:
  int sensorIndex;

public:
  SensorNotifyCallback(int index) : sensorIndex(index) {}

  void onConnect(BLEClient* pclient) override {
    MySerial.println("✅ [" + sensors[sensorIndex].name + "] BLE 서버에 연결됨!");
    
    // MTU 크기 협상 시도 (추가)
    if (pclient->setMTU(517)) {  // 최대 MTU 크기 설정 시도
      MySerial.println("📏 [" + sensors[sensorIndex].name + "] MTU 크기 증가 시도 완료");
    }
    
    sensors[sensorIndex].connected = true;
    
    // MQTT로 연결 상태 발행
    if (mqttClient.connected()) {
      String topic = mqttTopicPrefix + sensors[sensorIndex].name + "/status";
      mqttClient.publish(topic.c_str(), "connected", true);
    }
  }

  void onDisconnect(BLEClient* pclient) override {
    MySerial.println("❌ [" + sensors[sensorIndex].name + "] BLE 서버에서 연결 해제됨");
    sensors[sensorIndex].connected = false;
    resetSensorBuffer(sensorIndex);
    doScan = true;  // 재스캔 시작
    
    // MQTT로 연결 해제 상태 발행
    if (mqttClient.connected()) {
      String topic = mqttTopicPrefix + sensors[sensorIndex].name + "/status";
      mqttClient.publish(topic.c_str(), "disconnected", true);
    }
  }
};


// ====== 공통 알림 처리 함수 추가 ======
void processNotification(int sensorIndex, uint8_t* pData, size_t length) {
  String receivedChunk = String((char*)pData, length);
  MySerial.println("📤 [" + sensors[sensorIndex].name + "] 수신된 청크 (" + String(length) + " bytes): " + receivedChunk);
  
  sensors[sensorIndex].dataBuffer += receivedChunk;
  sensors[sensorIndex].lastDataTime = millis();
  
  // JSON 완성도 체크 (기존의 단순한 endsWith("}") 대신 정교한 파싱)
  if (isCompleteJson(sensors[sensorIndex].dataBuffer, sensorIndex)) {
    processCompleteJson(sensors[sensorIndex].dataBuffer, sensorIndex);
    resetSensorBuffer(sensorIndex);  // 기존의 단순 초기화 대신 전용 함수 사용
  }
  
  // 버퍼 크기 제한 (메모리 보호) - 새로 추가
  if (sensors[sensorIndex].dataBuffer.length() > 1000) {
    MySerial.println("⚠️ [" + sensors[sensorIndex].name + "] 버퍼 크기 초과, 초기화");
    resetSensorBuffer(sensorIndex);
  }
}

void notifyCallback1(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  processNotification(0, pData, length);  // 공통 함수로 간소화
}

void notifyCallback2(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  processNotification(1, pData, length);  // 공통 함수로 간소화
}

// ====== 장치 스캔 콜백 ======
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    MySerial.printf("📡 BLE 장치 발견: %s\n", advertisedDevice.toString().c_str());

    // 각 센서 노드 확인
    if (advertisedDevice.haveName()) {
      String deviceName = String(advertisedDevice.getName().c_str());
      for (int i = 0; i < NUM_SENSORS; i++) {
        if (deviceName == sensors[i].name && 
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
    MySerial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "연결됨" : "연결 안됨"));
    MySerial.println("MQTT: " + String(mqttClient.connected() ? "연결됨" : "연결 안됨"));
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      String status = sensors[i].connected ? "연결됨" : "연결 안됨";
      String bufferInfo = sensors[i].dataBuffer.length() > 0 ? 
                         " (버퍼: " + String(sensors[i].dataBuffer.length()) + " bytes)" : "";  // 추가
      MySerial.println(sensors[i].name + ": " + status + bufferInfo);  // 변경
    }
    MySerial.println("================\n");
    lastStatusPrint = currentTime;
  }
}

void setup() {
  // UART1 시작
  MySerial.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(1000);
  MySerial.println("=== BLE 다중 센서 수신기 + MQTT 게이트웨이 시작 ===");
  MySerial.println("✅ UART1 시작됨");

  // WiFi 연결
  connectToWiFi();
  
  // MQTT 클라이언트 설정
  mqttClient.setServer(mqttServerAddress, mqttPort);
  mqttClient.setKeepAlive(60);      // Keep-alive 60초
  mqttClient.setSocketTimeout(30);  // 소켓 타임아웃 30초
  
  MySerial.println("🔒 MQTT 클라이언트 보안 설정 완료");
  
  // 센서 노드 초기화
  initializeSensors();
  
  // BLE 초기화
  BLEDevice::init("BLE_MultiReceiver");
  BLEDevice::setMTU(517);
  MySerial.println("📏 MTU 크기 증가 시도 완료");

  MySerial.println("✅ BLE 초기화 완료");

  // 스캔 시작
  doScan = true;
  MySerial.println("🔍 타겟 장치 검색 중: SensorNode_1, SensorNode_2");
}

void loop() {
  // WiFi 연결 상태 확인
  if (WiFi.status() != WL_CONNECTED) {
    MySerial.println("⚠️ WiFi 연결 끊어짐, 재연결 시도...");
    connectToWiFi();
  }
  
  // MQTT 연결 확인 및 재연결
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    unsigned long currentTime = millis();
    if (currentTime - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      connectToMQTT();
      lastMqttReconnectAttempt = currentTime;
    }
  }
  
  // MQTT 클라이언트 루프 처리
  if (mqttClient.connected()) {
    mqttClient.loop();
  }

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
      resetSensorBuffer(i);  // 변경: 단순 초기화 대신 전용 함수 사용
    }
  }
}

  // 연결 상태 출력
  printConnectionStatus();
  
  delay(100);  // 100ms 대기
}