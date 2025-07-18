#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>

// ====== 핀 설정 ======
#define ONE_WIRE_BUS 4
#define UART_RX 44
#define UART_TX 43

// ====== UUID ======
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-90ab-cdef-1234567890ab"

// ====== 객체 선언 ======
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
HardwareSerial MySerial(1);
BLECharacteristic* pCharacteristic;
BLEServer* pServer;
BLEAdvertising* pAdvertising;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 1000; // 1초 간격

// ====== 연결 상태 콜백 클래스 ======
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    MySerial.println("✅ BLE 중앙 장치와 연결됨!");
    
    // MTU 크기 증가 요청 (최대 512바이트)
    pServer->updatePeerMTU(pServer->getConnId(), 512);
    MySerial.println("📡 MTU 크기 증가 요청됨");
  }
 
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    MySerial.println("❌ BLE 연결 해제됨");
  }
};

// ====== 온도 읽기 함수 ======
float readTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
 
  // 센서 오류 체크 (DS18B20은 오류 시 -127.00 또는 85.00 반환)
  if (tempC == DEVICE_DISCONNECTED_C || tempC == 85.0) {
    MySerial.println("⚠️ 온도 센서 오류 감지");
    return NAN; // Not a Number 반환
  }
 
  return tempC;
}

// ====== JSON 생성 함수 (전체 MAC 주소 사용) ======
String createJsonData(float temperature) {
  // 전체 MAC 주소를 그대로 사용
  String fullMac = String(BLEDevice::getAddress().toString().c_str());
  
  // 디버깅을 위해 전체 MAC 주소 출력
  MySerial.println("🔍 전체 MAC: " + fullMac);
  
  String json = "{\"mac\":\"" + fullMac + "\"";
 
  if (isnan(temperature)) {
    json += ",\"error\":\"sensor_error\"}";
  } else {
    json += ",\"temp\":" + String(temperature, 2) + "}";
  }
 
  return json;
}

// ====== 광고 시작 함수 ======
void startAdvertising() {
  pAdvertising->start();
  MySerial.println("📡 BLE 광고 시작됨");
}

void setup() {
  // UART1 시작
  MySerial.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(500);
  MySerial.println("=== BLE 온도 센서 노드 시작 ===");
  MySerial.println("✅ UART1 시작됨");
 
  // DS18B20 시작
  sensors.begin();
  int deviceCount = sensors.getDeviceCount();
  MySerial.printf("✅ DS18B20 센서 초기화됨 (감지된 센서: %d개)\n", deviceCount);
 
  if (deviceCount == 0) {
    MySerial.println("⚠️ 경고: DS18B20 센서가 감지되지 않음");
  }
 
  // BLE 초기화
  BLEDevice::init("SensorNode_1");
  
  // MTU 크기 설정
  BLEDevice::setMTU(512);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
 
  // 서비스 생성
  BLEService* pService = pServer->createService(SERVICE_UUID);
 
  // 특성 생성
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
 
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
 
  // 광고 설정
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);  // 최소 연결 간격
  pAdvertising->setMaxPreferred(0x12);  // 최대 연결 간격
 
  startAdvertising();
  MySerial.println("✅ BLE 초기화 완료");
  
  // 초기화 후 MAC 주소 확인
  MySerial.println("🔍 장치 MAC 주소: " + String(BLEDevice::getAddress().toString().c_str()));
}

void loop() {
  // 연결 상태 변화 감지
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // 연결 해제 후 잠시 대기
    startAdvertising(); // 재광고 시작
    oldDeviceConnected = deviceConnected;
  }
 
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
 
  // 센서 데이터 읽기 (1초 간격)
  unsigned long currentTime = millis();
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentTime;
   
    float tempC = readTemperature();
    String json = createJsonData(tempC);
   
    MySerial.println("📤 전송할 JSON: " + json + " (길이: " + String(json.length()) + ")");
   
    // 연결 상태에 따라 처리
    if (deviceConnected) {
      pCharacteristic->setValue(json.c_str());
      pCharacteristic->notify();
      MySerial.println("📡 Notify 전송됨");
    } else {
      if (isnan(tempC)) {
        MySerial.println("⏸ BLE 연결 없음, 센서 오류 발생");
      } else {
        MySerial.println("⏸ BLE 연결 없음, 현재 온도: " + String(tempC, 2) + "°C");
      }
    }
  }
 
  delay(100); // CPU 사용률 감소를 위한 짧은 대기
}