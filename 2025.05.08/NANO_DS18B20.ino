#include <SPI.h>               // Nano 33 IoT 의 Wi-Fi 칩 통신
#include <WiFiNINA.h>          // Wi-Fi 연결용
#include <PubSubClient.h>      // MQTT 클라이언트
#include <OneWire.h>           // DS18B20 통신
#include <DallasTemperature.h> // DS18B20 온도 측정

// ————— 네트워크 설정 —————
const char* ssid     = "배성준의 iPhone";
const char* password = "qwertyuioa";

// ————— MQTT 설정 —————
const char* mqtt_server = "test.mosquitto.org";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "MQTT/nds18";

// DS18B20 설정
const int ONE_WIRE_BUS = 2;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Wi-Fi & MQTT 클라이언트
WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // DS18B20 초기화
  sensors.begin();

  // Wi-Fi 연결
  connectWiFi();

  // MQTT 서버 설정
  mqttClient.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // MQTT 연결 확인, 끊겼으면 재접속
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // 온도 측정
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // DS18B20 연결 오류 체크
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 not connected!");
  } else {
    // 온도 출력
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println(" °C");

    // MQTT 퍼블리시 (숫자를 문자열로 변환)
    
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", tempC);
    mqttClient.publish(mqtt_topic, payload);
  }

  delay(10000); // 10초 간격으로 전송
}

// Wi-Fi 연결 함수
void connectWiFi() {
  Serial.print("Connecting to WiFi ");
  Serial.print(ssid);
  Serial.print(" ... ");
  int status = WiFi.begin(ssid, password);

  // 연결 대기
  while (status != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    status = WiFi.status();
  }
  Serial.println(" connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT 재접속 함수
void reconnectMQTT() {
  Serial.print("Connecting to MQTT...");
  // 클라이언트 ID로 보통 보드이름 또는 랜덤ID 사용
  String clientId = "Nano33IoT-";
  clientId += String(random(0xffff), HEX);

  // 연결 시도
  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" connected");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(1000);
    }
  }
}
