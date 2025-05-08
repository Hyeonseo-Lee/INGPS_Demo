#include <Adafruit_MAX31865.h>
#include "BluetoothSerial.h" // 내장 블루투스 사용

// MAX31865 설정 (소프트웨어 SPI)
Adafruit_MAX31865 thermo = Adafruit_MAX31865(5, 23, 19, 18);
#define RREF 430.0
#define RNOMINAL 100.0

volatile float Temperatura;

BluetoothSerial SerialBT;  // BluetoothSerial 객체 생성

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Temp"); // 블루투스 이름 설정

  Serial.println("Adafruit MAX31865 PT100 Sensor Test!");
  SerialBT.println("블루투스 연결됨: ESP32_Temp");

  thermo.begin(MAX31865_3WIRE);  // 3선식 RTD
}

void loop() {
  int start_time = millis();

  Temperatura = thermo.temperature(RNOMINAL, RREF);
  Temperatura -= 1.52; // 오프셋 보정

  // 시리얼 출력
  Serial.print("Temperatura: ");
  Serial.print(Temperatura);
  Serial.println(" °C");

  // 블루투스 출력
  SerialBT.print("Temperatura: ");
  SerialBT.print(Temperatura);
  SerialBT.println(" °C");

  int delay_time = (1000 / 16) - (millis() - start_time);
  if (delay_time > 0) {
    delay(delay_time);
  }
}