#include <Adafruit_MAX31865.h>
// #include "BluetoothSerial.h"  // ESP32 내장 블루투스 사용

// MAX31865 설정 (소프트웨어 SPI)
// 핀 순서: CS, DI(MOSI), DO(MISO), CLK
Adafruit_MAX31865 thermo = Adafruit_MAX31865(5, 23, 19, 18);

#define RREF 430.0     // 기준 저항값
#define RNOMINAL 100.0 // PT100 정격 저항 (0°C에서 100Ω)

// 블루투스 객체 생성
// BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  // SerialBT.begin("ESP32_Temp");  // 블루투스 이름

  Serial.println("MAX31865 PT100 Sensor Test");
  // SerialBT.println("블루투스 연결됨: ESP32_Temp");

  // RTD 3선식 사용
  thermo.begin(MAX31865_3WIRE);
}

void loop() {
  // RTD의 raw 값 읽기
  uint16_t rtd_raw = thermo.readRTD();

  // 저항값 계산
  float resistance = rtd_raw;
  resistance /= 32768;
  resistance *= RREF;

  // 시리얼 모니터 출력
  Serial.print("RTD Raw: ");
  Serial.println(rtd_raw);
  Serial.print("Calculated Resistance (Ohms): ");
  Serial.println(resistance, 2);

  // 블루투스 출력
  // SerialBT.print("Resistance: ");
  // SerialBT.print(resistance, 2);
  // SerialBT.println(" Ohms");

  delay(1000);  // 1초 대기
}
