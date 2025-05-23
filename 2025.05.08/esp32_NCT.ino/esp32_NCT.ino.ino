#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include <OneWire.h>            // DS18B20 통신
#include <DallasTemperature.h>  // DS18B20 온도 측정


// NTC Thermistor configuration
#define NTC_PIN       36        // Analog pin connected to thermistor divider
#define R1            9000.0    // Series resistor value [Ω]
#define C1            1.290556126e-03  // Steinhart-Hart coefficient C1
#define C2            2.066785119e-04  // Steinhart-Hart coefficient C2
#define C3            2.046300710e-07  // Steinhart-Hart coefficient C3

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// 단일 MQTT 서버 (두 번째 서버 사용)
const char* mqttServerAddress = "9c500c1053df40c795c005da44aee8f0.s2.eu.hivemq.cloud";
const int mqttPort = 8883;
String macAddress;
float filteredTemp = NAN;

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

void setup() {
  Serial.begin(115200);
  
  connectWiFi();
  macAddress = WiFi.macAddress();
  mqttClient.setServer(mqttServerAddress, mqttPort);
  wifiClient.setCACert(root_ca);
  // 첫 측정으로 필터 초기화
  filteredTemp = readNTCTemp();
  Serial.println("Setup complete, MQTT initializing...");
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
      Serial.println("MQTT connected");
    } else {
      Serial.printf("MQTT connection failed (%d), retry in 5s\n", mqttClient.state());
      delay(5000);
    }
  }
}

float readNTCTemp() {
  int raw = analogRead(NTC_PIN);
  float voltage = raw * (3.3 / 4095.0);
  float resistance = (3.3 * R1 / voltage) - R1;
  float logR = log(resistance);
  float invT = C1 + C2 * logR + C3 * logR * logR * logR;
  float kelvin = 1.0 / invT;
  return kelvin - 273.15;
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  float temp = readNTCTemp();
  filteredTemp = isnan(filteredTemp) ? temp : (filteredTemp * 0.9 + temp * 0.1);

  StaticJsonDocument<128> doc;
  doc["macAddress"] = macAddress;
  doc["temperatureCelsius"] = filteredTemp;
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish("MQTT/ntc", buf);

  Serial.printf("MAC: %s, Temp: %.2f C\n", macAddress.c_str(), filteredTemp);
  delay(500);
}
