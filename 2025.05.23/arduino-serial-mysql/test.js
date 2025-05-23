// 1) 모듈 불러오기
const { SerialPort } = require("serialport");
const { ReadlineParser } = require("@serialport/parser-readline");
const mysql = require("mysql2");

// 2) 시리얼 포트 설정 (윈도우: "COM3", mac/linux: "/dev/ttyUSB0" 등)
const port = new SerialPort({
  path: "COM3", // 실제 연결된 포트 이름으로 바꿔주세요
  baudRate: 115200,
});

// 3) 줄 단위 파서 생성
const parser = port.pipe(new ReadlineParser({ delimiter: "\r\n" }));

// 4) MySQL 연결 풀 생성
const db = mysql.createPool({
  host: "localhost", // 로컬이면 localhost, 원격이면 IP나 도메인
  user: "root", // MySQL 계정
  password: "1234", // 해당 계정의 비밀번호
  database: "templine", // 사용할 스키마 이름
  waitForConnections: true,
  connectionLimit: 5,
});

// 5) 시리얼 포트 열림/오류 이벤트
port.on("open", () => {
  console.log("✅ 시리얼 포트 열림:", port.path);
});
port.on("error", (err) => {
  console.error("❌ 시리얼 포트 오류:", err.message);
});

// 6) 시리얼 데이터 수신 시 처리
parser.on("data", (line) => {
  // 수신된 문자열을 실수로 변환
  const temp = parseFloat(line);
  if (isNaN(temp)) {
    console.warn("⚠️ 수신된 값이 숫자가 아닙니다:", line);
    return;
  }

  console.log(`읽은 온도: ${temp} °C ➔ DB 저장 시도…`);

  // DB에 INSERT
  db.execute(
    "INSERT INTO ds18b20 (temperature) VALUES (?)",
    [temp],
    (err, results) => {
      if (err) {
        console.error("❌ DB 저장 오류:", err);
      } else {
        console.log("✅ 저장 완료, ID:", results.insertId);
      }
    }
  );
});
