// index.js
// 1) 모듈 불러오기
const mqtt = require("mqtt");
const mysql = require("mysql2");
const express = require("express");
const cors = require("cors");
const os = require("os");

// 2) MQTT 브로커 연결 설정
const BROKER_URL =
  "mqtts://9c500c1053df40c795c005da44aee8f0.s2.eu.hivemq.cloud:8883";

const mqttClient = mqtt.connect(BROKER_URL, {
  protocol: "mqtts",
  username: "HyeonseoLee", // HiveMQ Cloud Username
  password: "1234qazA", // HiveMQ Cloud Password
  rejectUnauthorized: true, // TLS 인증서 검증 활성화
  clientId: "NodeJS_Server_" + Math.random().toString(16).substr(2, 8),
  clean: true,
  keepalive: 60,
  reconnectPeriod: 5000,
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

// 센서 데이터 저장 객체
const sensorData = {
  SensorNode_1: {
    lastData: null,
    lastUpdate: null,
    status: "unknown",
  },
  SensorNode_2: {
    lastData: null,
    lastUpdate: null,
    status: "unknown",
  },
  gateway: {
    status: "unknown",
  },
};

// 4) MQTT 연결 이벤트 처리
mqttClient.on("connect", () => {
  console.log("✅ MQTT 브로커 연결됨:", BROKER_URL);

  // 새로운 토픽들 구독
  const topics = [
    "sensors/+/data", // 모든 센서 데이터 (SensorNode_1, SensorNode_2)
    "sensors/+/status", // 모든 센서 상태
    "sensors/gateway/status", // 게이트웨이 상태
  ];

  topics.forEach((topic) => {
    mqttClient.subscribe(topic, (err) => {
      if (err) {
        console.error(`❌ 토픽 구독 실패: ${topic}`, err);
      } else {
        console.log(`✅ 토픽 구독 성공: ${topic}`);
      }
    });
  });
});

// 5) MQTT 오류 처리
mqttClient.on("error", (err) => {
  console.error("❌ MQTT 오류:", err.message);
});

// 재연결 처리
mqttClient.on("reconnect", () => {
  console.log("🔄 MQTT 재연결 시도 중...");
});

mqttClient.on("close", () => {
  console.log("⚠️ MQTT 연결이 해제됨");
});

// 6) MQTT 메시지 수신 이벤트
mqttClient.on("message", (topic, message) => {
  const timestamp = new Date().toLocaleString("ko-KR", {
    timeZone: "Asia/Seoul",
    hour12: false,
  });

  console.log(`\n📨 [${timestamp}] 메시지 수신`);
  console.log(`📍 토픽: ${topic}`);

  try {
    const messageStr = message.toString();
    console.log(`📥 페이로드: ${messageStr}`);

    // 토픽 파싱
    const topicParts = topic.split("/");

    if (topicParts[0] === "sensors") {
      if (topicParts[1] === "gateway") {
        // 게이트웨이 상태: sensors/gateway/status
        console.log(`🚪 게이트웨이 상태: ${messageStr}`);
        sensorData.gateway.status = messageStr;
      } else if (topicParts[2] === "data") {
        // 센서 데이터: sensors/SensorNode_1/data, sensors/SensorNode_2/data
        const sensorName = topicParts[1]; // SensorNode_1 또는 SensorNode_2

        try {
          const data = JSON.parse(messageStr);
          console.log(`🌡️ [${sensorName}] JSON 데이터:`, data);

          // 센서 데이터 저장
          sensorData[sensorName].lastData = data;
          sensorData[sensorName].lastUpdate = timestamp;

          // 데이터 처리 및 DB 저장
          processSensorData(sensorName, data, timestamp);
        } catch (parseError) {
          console.error(`❌ [${sensorName}] JSON 파싱 오류:`, parseError);
        }
      } else if (topicParts[2] === "status") {
        // 센서 상태: sensors/SensorNode_1/status, sensors/SensorNode_2/status
        const sensorName = topicParts[1];
        console.log(`📊 [${sensorName}] 상태: ${messageStr}`);
        sensorData[sensorName].status = messageStr;
      }
    }

    // 전체 상태 출력
    printCurrentStatus();
  } catch (error) {
    console.error("❌ 메시지 처리 오류:", error);
  }
});

// 센서 데이터 처리 함수
function processSensorData(sensorName, data, timestamp) {
  if (data.error) {
    console.log(`⚠️ [${sensorName}] 센서 오류: ${data.error}`);
    return;
  }

  // JSON 구조: {"mac":"94:a9:90:1c:xx:xx","temp":25.3}
  const mac = data.mac;
  const temp = parseFloat(data.temp);

  console.log(`  ⏰ timestamp: ${timestamp}`);
  console.log(`  📡 mac      : ${mac}`);
  console.log(`  🌡️ temperature: ${temp}°C`);
  console.log(`  📱 sensor   : ${sensorName}`);

  if (!mac || isNaN(temp)) {
    console.warn("⚠️ 유효하지 않은 데이터:", data);
    return;
  }

  // 온도 임계값 체크
  if (temp > 30.0) {
    console.log(`🔥 [${sensorName}] 고온 경고! 온도가 30°C를 초과했습니다.`);
  }

  // DB 저장 (기존 방식)
  db.execute(
    "INSERT INTO ds18b20 (temperature, timestamp, mac) VALUES (?, ?, ?)",
    [temp, timestamp, mac],
    (err, results) => {
      if (err) {
        console.error(`❌ [${sensorName}] DB 저장 오류:`, err);
      } else {
        console.log(`✅ [${sensorName}] 저장 완료, ID: ${results.insertId}`);
      }
    }
  );
}

// 현재 상태 출력
function printCurrentStatus() {
  console.log("\n=== 현재 상태 ===");
  console.log(`게이트웨이: ${sensorData.gateway.status}`);

  Object.keys(sensorData).forEach((key) => {
    if (key !== "gateway") {
      const sensor = sensorData[key];
      const lastTemp = sensor.lastData ? sensor.lastData.temp : "N/A";
      const lastUpdate = sensor.lastUpdate || "N/A";

      console.log(
        `${key}: ${sensor.status} | 온도: ${lastTemp}°C | 마지막 업데이트: ${lastUpdate}`
      );
    }
  });
  console.log("================\n");
}

// ── HTTP API 서버 설정 (Express) ── //

const app = express();
app.use(cors()); // CORS 허용
app.use(express.json()); // JSON 바디 파싱

/**
 * GET /api/temperatures/latest
 * 최근 1개 데이터를 시간 내림차순으로 조회
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

      res.json(results[0]);
    }
  );
});

/**
 * GET /api/temperatures/all
 * 모든 센서의 최근 데이터 조회
 */
app.get("/api/temperatures/all", (req, res) => {
  const limit = parseInt(req.query.limit) || 100;

  db.query(
    "SELECT temperature, timestamp, mac FROM ds18b20 ORDER BY timestamp DESC LIMIT ?",
    [limit],
    (err, results) => {
      if (err) {
        console.error("❌ DB 조회 오류:", err);
        return res.status(500).json({ error: err.message });
      }

      res.json(results);
    }
  );
});

/**
 * GET /api/sensors/status
 * 현재 센서 상태 조회
 */
app.get("/api/sensors/status", (req, res) => {
  res.json(sensorData);
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

  console.log("\n📡 구독 중인 MQTT 토픽:");
  console.log("  - sensors/+/data (모든 센서 데이터)");
  console.log("  - sensors/+/status (모든 센서 상태)");
  console.log("  - sensors/gateway/status (게이트웨이 상태)");
});

// 정기적인 상태 체크 (2분마다)
setInterval(() => {
  console.log("\n📊 정기 상태 체크");
  printCurrentStatus();

  // 데이터가 너무 오래되었는지 체크 (5분)
  const now = new Date();
  Object.keys(sensorData).forEach((key) => {
    if (key !== "gateway" && sensorData[key].lastUpdate) {
      const lastUpdate = new Date(sensorData[key].lastUpdate);
      const timeDiff = (now - lastUpdate) / 1000 / 60; // 분 단위

      if (timeDiff > 5) {
        console.log(
          `⚠️ [${key}] 데이터가 ${timeDiff.toFixed(1)}분 동안 수신되지 않음`
        );
      }
    }
  });
}, 120000);

// 프로그램 종료 시 정리
process.on("SIGINT", () => {
  console.log("\n🛑 프로그램 종료 중...");
  mqttClient.end();
  db.end();
  process.exit();
});
