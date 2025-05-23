// index.js

// 1) 모듈 불러오기
const mqtt = require("mqtt");
const mysql = require("mysql2");

// 2) MQTT 브로커 연결 설정
const BROKER_URL =
  "mqtts://9c500c1053df40c795c005da44aee8f0.s2.eu.hivemq.cloud:8883";
// Arduino 스케치에서 publish 하는 토픽과 동일하게!
const TOPIC = "MQTT/ntc";

const client = mqtt.connect(BROKER_URL, {
  protocol: "mqtts",
  username: "HyeonseoLee", // HiveMQ Cloud Username
  password: "1234qazA", // HiveMQ Cloud Password
  rejectUnauthorized: true, // TLS 인증서 검증 활성화
});

// 3) MySQL 연결 풀 설정
const db = mysql.createPool({
  host: "localhost", // MySQL 서버 호스트
  user: "root", // MySQL 계정
  password: "1234", // MySQL 비밀번호
  database: "templine", // 사용할 스키마
  waitForConnections: true,
  connectionLimit: 5,
});

// 4) MQTT 연결 이벤트 처리
client.on("connect", () => {
  console.log("✅ MQTT 브로커 연결됨:", BROKER_URL);
  client.subscribe(TOPIC, (err) => {
    if (err) console.error("❌ 토픽 구독 실패:", err);
    else console.log(`✅ 토픽 구독 성공: ${TOPIC}`);
  });
});

// 5) MQTT 오류 처리
client.on("error", (err) => {
  console.error("❌ MQTT 오류:", err.message);
});

// 6) MQTT 메시지 수신 이벤트
client.on("message", (topic, message) => {
  // 지정한 토픽만 처리
  if (topic !== TOPIC) return;

  const payload = message.toString();
  console.log("📥 수신 — Topic:", topic, "Payload:", payload);

  let data;
  try {
    data = JSON.parse(payload);
  } catch (e) {
    console.warn("⚠️ JSON 파싱 실패:", payload);
    return;
  }

  const mac = data.macAddress;
  const temp = parseFloat(data.temperatureCelsius);

  if (!mac || isNaN(temp)) {
    console.warn("⚠️ 유효하지 않은 데이터:", data);
    return;
  }

  console.log(`MAC: ${mac}, Temp: ${temp} °C ➔ DB 저장 시도…`);

  // DB에 저장
  db.execute(
    "INSERT INTO ds18b20 (mac, temperature) VALUES (?, ?)",
    [mac, temp],
    (err, results) => {
      if (err) {
        console.error("❌ DB 저장 오류:", err);
      } else {
        console.log("✅ 저장 완료, ID:", results.insertId);
      }
    }
  );
});
