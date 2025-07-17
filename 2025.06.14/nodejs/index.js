// index.js

// 1) 모듈 불러오기
const mqtt = require("mqtt");
const mysql = require("mysql2");
const express = require("express");
const cors = require("cors");
// index.js 맨 위에 추가
const os = require("os");

// 2) MQTT 브로커 연결 설정
const BROKER_URL =
  "mqtts://9c500c1053df40c795c005da44aee8f0.s2.eu.hivemq.cloud:8883";
const TOPIC = "MQTT/ntc";

const mqttClient = mqtt.connect(BROKER_URL, {
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
mqttClient.on("connect", () => {
  console.log("✅ MQTT 브로커 연결됨:", BROKER_URL);
  mqttClient.subscribe(TOPIC, (err) => {
    if (err) console.error("❌ 토픽 구독 실패:", err);
    else console.log(`✅ 토픽 구독 성공: ${TOPIC}`);
  });
});

// 5) MQTT 오류 처리
mqttClient.on("error", (err) => {
  console.error("❌ MQTT 오류:", err.message);
});

// 6) MQTT 메시지 수신 이벤트
mqttClient.on("message", (topic, message) => {
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

  const mac = data.macAddress; // MAC 주소
  const temp = parseFloat(data.temperatureCelsius); // 온도
  const timestamp = new Date().toLocaleString({
    // 서울 시간 기준 문자열
    timeZone: "Asia/Seoul",
    hour12: false,
  });

  console.log("  ⏰ timestamp:", timestamp);
  console.log("  📡 mac      :", mac);
  console.log("  🌡️ temperature:", temp);

  if (!mac || isNaN(temp)) {
    console.warn("⚠️ 유효하지 않은 데이터:", data);
    return;
  }

  // DB 저장
  db.execute(
    "INSERT INTO ds18b20 (temperature, timestamp, mac) VALUES (?, ?, ?)",
    [temp, timestamp, mac],
    (err, results) => {
      if (err) {
        console.error("❌ DB 저장 오류:", err);
      } else {
        console.log("✅ 저장 완료, ID:", results.insertId);
      }
    }
  );
});

// ── HTTP API 서버 설정 (Express) ── //

const app = express();
app.use(cors()); // CORS 허용
app.use(express.json()); // JSON 바디 파싱

/**
 * GET /api/temperatures
 * 최근 N개(여기선 100) 데이터를 시간 내림차순으로 조회
 */
app.get("/api/temperatures/latest", (req, res) => {
  db.query(
    "SELECT temperature, timestamp, mac FROM ds18b20 ORDER BY timestamp DESC LIMIT 1",
    (err, results) => {
      if (err) {
        console.error("❌ DB 조회 오류:", err);
        return res.status(500).json({ error: err.message });
      }

      if (results.length === 0) {
        return res.status(404).json({ error: "데이터 없음" });
      }

      // 단일 객체로 반환
      res.json(results[0]);
    }
  );
});
// 7) HTTP 서버 실행
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`✅ HTTP API 서버 실행 중: http://localhost:${PORT}`);

  // 네트워크 인터페이스에서 외부 접속용 IP 출력
  const nets = os.networkInterfaces();
  for (const name of Object.keys(nets)) {
    for (const net of nets[name]) {
      if (net.family === "IPv4" && !net.internal) {
        console.log(`🌐 외부 접속 URL: http://${net.address}:${PORT}`);
      }
    }
  }
});
