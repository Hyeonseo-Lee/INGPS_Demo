#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ————— DS18B20 설정 —————
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// ————— MQTT & 보안 클라이언트 —————
WiFiClientSecure wifiClient;
PubSubClient    mqttClient(wifiClient);

// ————— MQTT 서버 정보 —————
const char* mqttServerAddress = "9c500c1053df40c795c005da44aee8f0.s2.eu.hivemq.cloud";
const int   mqttPort          = 8883;

// ————— 시스템 전역변수 —————
String macAddress;

// Root CA certificate (truncated)
static const char *root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";




// ————— 함수 선언 —————
void connectWiFi();
void connectMQTT();
float readDS18B20Temp();

void setup() {
  Serial.begin(115200);

  // 1) DS18B20 초기화
  ds18b20.begin();

  // 2) Wi-Fi 연결 & MAC 주소 읽기
  connectWiFi();
  macAddress = WiFi.macAddress();
  Serial.printf("Device MAC: %s\n", macAddress.c_str());

  // 3) MQTT 설정
  mqttClient.setServer(mqttServerAddress, mqttPort);
  wifiClient.setCACert(root_ca);  // 필요 없으면 주석 처리 가능
}

void loop() {
  // 1) Wi-Fi/MQTT 연결 유지
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected())    connectMQTT();
  mqttClient.loop();

  // 2) DS18B20 온도 읽기
  float temp = readDS18B20Temp();
  if (!isnan(temp)) {
    // 3) JSON 문서 생성
    StaticJsonDocument<128> doc;
    doc["macAddress"]          = macAddress;
    doc["temperatureCelsius"]  = temp;
    char payload[128];
    serializeJson(doc, payload);

    // 4) MQTT 퍼블리시
    mqttClient.publish("MQTT/ntc", payload);

    // 5) 디버그 출력
    Serial.printf("MAC: %s, Temp: %.2f °C\n",
                  macAddress.c_str(), temp);
  }

  delay(500);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to WiFi \"%s\"", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.printf("\nWiFi connected! IP: %s\n",
                WiFi.localIP().toString().c_str());
}

void connectMQTT() {
  Serial.print("Connecting to MQTT...");
  while (!mqttClient.connected()) {
    // 클라이언트 ID를 랜덤하게 생성
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(),
                           mqttUsername, mqttPassword)) {
      Serial.println(" connected");
    } else {
      Serial.printf(" failed, rc=%d. retry in 5s\n",
                    mqttClient.state());
      delay(5000);
    }
  }
}

float readDS18B20Temp() {
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 not connected!");
    return NAN;
  }
  return t;
}
