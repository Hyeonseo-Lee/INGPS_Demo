#include <WiFi.h>               // ESP32 Wi-Fi
#include <PubSubClient.h>       // MQTT 클라이언트
#include <OneWire.h>            // DS18B20 통신
#include <DallasTemperature.h>  // DS18B20 온도 측정

// ————— 네트워크 설정 —————
const char* ssid     = "배성준의 iPhone";
const char* password = "qwertyuioa";

// ————— MQTT 설정 —————
const char* mqtt_server = "test.mosquitto.org";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "MQTT/nds18";

// DS18B20 설정 (데이터 핀을 GPIO 4로 연결)
const int ONE_WIRE_BUS = 4;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Wi-Fi & MQTT 클라이언트
WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);

void setup() {
  Serial.begin(115200);
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

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 not connected!");
  } else {
    // Serial.printf 대체
    Serial.print("Temperature: ");
    Serial.print(tempC, 2);
    Serial.println(" °C");

    // MQTT 퍼블리시
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", tempC);
    mqttClient.publish(mqtt_topic, payload);
  }

  delay(10000); // 10초 간격 전송
}

// — Wi-Fi 연결 함수
void connectWiFi() {
  // Serial.printf 대체
  Serial.print("Connecting to WiFi \"");
  Serial.print(ssid);
  Serial.println("\" ... ");

  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    // 20초 내 미연결 시 재시도
    if (millis() - start > 20000) {
      Serial.println();
      Serial.println("WiFi reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      start = millis();
    }
  }
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// — MQTT 재접속 함수 (상태 코드 확인)
void reconnectMQTT() {
  Serial.print("Connecting to MQTT...");
  String clientId = "ESP32-";
  clientId += String((uint32_t)random());

  while (!mqttClient.connected()) {
    int8_t state = mqttClient.connect(clientId.c_str());
    // Serial.printf 대체
    Serial.print(" state=");
    Serial.println(state);
    if (state == 0) {
      Serial.println("MQTT connected!");
      break;
    }
    Serial.println("Retry in 5s");
    delay(500);
  }
}