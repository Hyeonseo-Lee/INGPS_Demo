//#include <ESP8266WiFi.h> //ESP8266꺼
#include <WiFi.h> //ESP32꺼
#include <PubSubClient.h>

//ESP32가 인터넷 공유기와 연결되어야겠지?
const char* ssid = "배성준의 iPhone";
const char* password = "qwertyuioa";
//MQTT서버의 주소
const char* mqtt_server = "broker.emqx.io"; //해외 공용 공짜 브로커

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

//NTC서미스터 부분 시작
#define NTC_PIN 36 //서미스터가 연결된 핀번호
#define R1 9000 //저항의크기(10k옴 = 10000옴)
//공식을 위한 상수 3종세트
//아~! 나중에 온도값을 보정한다면 C1,C2,C3값이 변하면 되는것이다!
#define c1 1.290556126e-03
#define c2 2.066785119e-04
#define c3 2.046300710e-07

float logR2, R2, T, Tc, Tf;

float filltered_temp = 0; //나는 초기값이 뭐지?
//NTC서미스터 부분 종료

//인터넷 공유기와 접속하는 부분
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//ESP32의 메시지 수신부
//ESP32는 온도값을 전송만하니깐 별로 필요없겠지?
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

//ESP32가 MQTT서버와 연결하는부분
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      //MQTT서버와 연결완료후 구독토픽 신청하는 부분
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  //최초 1회 필터변수에 초기값을 준다
  int Vo = analogRead(NTC_PIN);
  //계산하는 공식(그냥 사용하도록 하자)
  R2 = R1 * (4095.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  //최종결과
  Tc = T - 273.15; //온도(도씨)
  filltered_temp = Tc;
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  //MQTT서버와 연결을 유지하기 위한 부분
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //2000밀리초 간격으로 어떤 메시지를 발행하는 예제(기본)
  //500밀리초 간격으로 수정
  unsigned long now = millis(); //지우지말기
  if (now - lastMsg > 500) { //지우지말기
    lastMsg = now; //지우지말기
    //온도를 측정한다
    int Vo = analogRead(NTC_PIN);

    //계산하는 공식(그냥 사용하도록 하자)
    R2 = R1 * (4095.0 / (float)Vo - 1.0);
    logR2 = log(R2);
    T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
    //최종결과
    Tc = T - 273.15; //온도(도씨)
    //Tf = (Tc * 9.0)/ 5.0 + 32.0; //온도(화씨) 
  
    filltered_temp = (filltered_temp * 0.9) + (Tc * 0.1);

    //filltered_temp : FLOAT
    //filltered_temp를 String으로 바꾼다
    //String으로 바뀐 온도값을 char배열로 바꾼다
    Serial.println(filltered_temp);
    client.publish("MQTT/ntc", String(filltered_temp).c_str());
  }//지우지말기
}
