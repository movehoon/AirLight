/*
 * ESP8266 강좌 시리즈
 * 미제먼지 데이터 받아오기 3편
 * 
 * 본 저작물은 '한국환경공단'에서 실시간 제공하는 '한국환경공단_대기오염정보'를 이용하였습니다.
 * https://www.data.go.kr/dataset/15000581/openapi.do
 */

#include <WiFi.h> // ESP 8266 와이파이 라이브러리
#include <HTTPClient.h> // HTTP 클라이언트
#include <Adafruit_NeoPixel.h> // 네오픽셀 라이브러리

// ----- Bluetooth Header -----//
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
// ----- Bluetooth Header -----//

#include <Preferences.h>

#define PREFS_NAME "saved_data"
#define PREFS_MODE "MODE"
#define PREFS_AP "AP"
#define PREFS_PW "PW"
#define PREFS_SI "SI"

#define PIN_AP_SET 0
#define PIN_NEOPIXEL 2
uint8_t pin_ap_set_prev;

#define NUMPIXELS      8 // 네오픽셀 LED 수
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800); // 네오픽셀 객체

Preferences prefs;

uint8_t mode;
String ap;
String pw;
String si;


//String sido = "서울"; // 서울, 부산, 대구, 인천, 광주, 대전, 울산, 경기, 강원, 충북, 충남, 전북, 전남, 경북, 경남, 제주, 세종 중 입력
String key = "XqmG8pqiJv2w7Tp8egf1FDxpwWe4a4RKHq4nCaCyjxE1QvoPY8VoBF0oY6Gx9TJ2vQ%2BaTZVBk0RTdjXZsmMx5Q%3D%3DXqmG8pqiJv2w7Tp8egf1FDxpwWe4a4RKHq4nCaCyjxE1QvoPY8VoBF0oY6Gx9TJ2vQ%2BaTZVBk0RTdjXZsmMx5Q%3D%3D";
//String url = "http://openapi.airkorea.or.kr/openapi/services/rest/ArpltnInforInqireSvc/getCtprvnMesureSidoLIst?sidoName=" + si + "&searchCondition=HOUR&pageNo=1&numOfRows=200&ServiceKey=" + key;
String url_first = "http://openapi.airkorea.or.kr/openapi/services/rest/ArpltnInforInqireSvc/getCtprvnMesureSidoLIst?sidoName=";
String url_second = "&searchCondition=HOUR&pageNo=1&numOfRows=200&ServiceKey=";

float so2, co, o3, no2, pm10, pm25 = 0; // 대기오염정보 데이터값
int score = 0; // 대기오염점수 0-최고 7-최악

// ----- Bluetooth Variables -----//
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
// ----- Bluetooth Variables -----//


void setup()
{
  // 시리얼 세팅
  Serial.begin(115200);
  Serial.println("Start");

  pinMode(PIN_AP_SET, INPUT_PULLUP);

  // 모드 확인
  prefs.begin(PREFS_NAME);
  mode = prefs.getChar(PREFS_MODE, -1);
  ap = prefs.getString(PREFS_AP, "");
  pw = prefs.getString(PREFS_PW, "");
  si = prefs.getString(PREFS_SI, "서울");

  // 네오픽셀 초기화
  pixels.begin();
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, random(0xffffff));
  }
  pixels.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)
  pixels.show();

  if (mode==0) {
    // WiFi 모드
    setupWifi();
  }
  else if (mode==1) {
    setupBT();
  }
  else {
    // 초기화 모드
    mode = 0;
    Serial.printf("Set mode to %d\n", mode);
    prefs.putChar(PREFS_MODE, mode);
    ESP.restart();
  }
}

void loop() {

  CheckModeChange();

  if (mode==0) {
    loopWifi();
  }
  else if (mode == 1) {
    loopBT();
  }
}

float getNumber(String str, String tag, int from) {
  int f = str.indexOf(tag, from) + tag.length(); // 테그의 위치와 테그의 문자 길이의 합
  int t = str.indexOf("<", f); // 다음 테스시작위치
  String s = str.substring(f, t); // 테그 사이의 숫자를 문자열에 저장
  return s.toFloat(); // 문자를 소수로 변환 후 반환
}

int getScore() {
  int s = -1;
  if (pm10 >= 151 || pm25 >= 76 || o3 >= 0.38 || no2 >= 1.1 || co >= 32 || so2 > 0.6) // 최악
    s = 7;
  else if (pm10 >= 101 || pm25 >= 51 || o3 >= 0.15 || no2 >= 0.2 || co >= 15 || so2 > 0.15) // 매우 나쁨
    s = 6;
  else if (pm10 >= 76 || pm25 >= 38 || o3 >= 0.12 || no2 >= 0.13 || co >= 12 || so2 > 0.1) // 상당히 나쁨
    s = 5;
  else if (pm10 >= 51 || pm25 >= 26 || o3 >= 0.09 || no2 >= 0.06 || co >= 9 || so2 > 0.05) // 나쁨
    s = 4;
  else if (pm10 >= 41 || pm25 >= 21 || o3 >= 0.06 || no2 >= 0.05 || co >= 5.5 || so2 > 0.04) // 보통
    s = 3;
  else if (pm10 >= 31 || pm25 >= 16 || o3 >= 0.03 || no2 >= 0.03 || co >= 2 || so2 > 0.02) // 양호
    s = 2;
  else if (pm10 >= 16 || pm25 >= 9 || o3 >= 0.02 || no2 >= 0.02 || co >= 1 || so2 > 0.01) // 좋음
    s = 1;
  else // 최고
    s = 0;
  return s;
}

void setLEDColor(int s) {
  int color;
  if (s == 0) // 최고
    color = pixels.Color(0, 63, 255);
  else if (s == 1) // 좋음
    color = pixels.Color(0, 127, 255);
  else if (s == 2) // 양호
    color = pixels.Color(0, 255, 255);
  else if (s == 3) // 보통
    color = pixels.Color(0, 255, 63);
  else if (s == 4) // 나쁨
    color = pixels.Color(255, 127, 0);
  else if (s == 5) // 상당히 나쁨
    color = pixels.Color(255, 63, 0);
  else if (s == 6) // 매우 나쁨
    color = pixels.Color(255, 31, 0);
  else // 최악
    color = pixels.Color(255, 0, 0);
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
}

void CheckModeChange() {
  if (!digitalRead(PIN_AP_SET) && pin_ap_set_prev) {
    mode = !mode;
    Serial.printf("Set mode to %d\n", mode);
    prefs.putChar(PREFS_MODE, mode);
    ESP.restart();
  }
  pin_ap_set_prev = digitalRead(PIN_AP_SET);
}

void setupWifi() {
  // 와이파이 접속
  WiFi.begin(ap.c_str(), pw.c_str()); // 공유기 이름과 비밀번호

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) // 와이파이 접속하는 동안 "." 출력
  {
    delay(500);
    Serial.print(".");
    CheckModeChange();
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP()); // 접속된 와이파이 주소 출력  
}

void loopWifi() {
  if (WiFi.status() == WL_CONNECTED) // 와이파이가 접속되어 있는 경우
  {
    WiFiClient client; // 와이파이 클라이언트 객체
    HTTPClient http; // HTTP 클라이언트 객체
    http.setTimeout(3000);
    http.begin(url_first + si + url_second + key);
    // 서버에 연결하고 HTTP 헤더 전송
    int httpCode = http.GET();
    // httpCode 가 음수라면 에러
    if (httpCode > 0) { // 에러가 없는 경우
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString(); // 받은 XML 데이터를 String에 저장
        Serial.printf("[HTTP] result: %s\n", payload.c_str());
        int cityIndex = payload.indexOf("서초");
        so2 = getNumber(payload, "<so2Value>", cityIndex);
        co = getNumber(payload, "<coValue>", cityIndex);
        o3 = getNumber(payload, "<o3Value>", cityIndex);
        no2 = getNumber(payload, "<no2Value>", cityIndex);
        pm10 = getNumber(payload, "<pm10Value>", cityIndex);
        pm25 = getNumber(payload, "<pm25Value>", cityIndex);
      }
    } else {
      Serial.printf("[HTTP] GET... 실패, 에러코드: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    score = getScore(); // score 변수에 대기오염점수 저장
    Serial.println(score); // 시리얼로 출력
    setLEDColor(score); // 점수에 따라 LED 색상 출력
    delay(5000);
  }
}

// ----- Bluetooth Code -----//
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        char *cmd_line = (char *)malloc(rxValue.length()+1);
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
          cmd_line[i] = rxValue[i];
        }
        cmd_line[rxValue.length()] = 0;
        Serial.printf(", length: %d\n", rxValue.length());
        parse(cmd_line);
        free(cmd_line);

        Serial.println("*********");
      }
    }
};

char msg[256];
void parse(char* cmd) {
  Serial.print("parsing: ");
  Serial.println(cmd);
  
  if (strlen(cmd)==0)
    return;
  
  char *p_token = (char *)strtok(cmd, "=");
  
  if (strncmp(p_token, "#", 1) == 0) {
    // Ignore comment
  }
  else if (strncmp(p_token,"mo",2)==0) {
    p_token = (char *)strtok(NULL, "=");
    if (p_token) {
      if (p_token[0] == '1')
        mode = 1;
      else
        mode = 0;
      prefs.putChar(PREFS_MODE, mode);
    }
    sprintf(msg, "mo=%d\n", mode);
    printf(msg);
    pTxCharacteristic->setValue((uint8_t *)msg, strlen(msg));
    pTxCharacteristic->notify();
  }
  else if (strncmp(p_token,"ap",2)==0) {
    p_token = (char *)strtok(NULL, "=");
    if (p_token) {
      prefs.putString(PREFS_AP, p_token);
      ap = p_token;
    }
    sprintf(msg, "ap=%s\n", ap);
    printf(msg);
    pTxCharacteristic->setValue((uint8_t *)msg, strlen(msg));
    pTxCharacteristic->notify();
  }
  else if (strncmp(p_token,"pw",2)==0) {
    p_token = (char *)strtok(NULL, "=");
    if (p_token) {
      prefs.putString(PREFS_PW, p_token);
      pw = p_token;
    }
    sprintf(msg, "pw=%s\n", pw);
    printf(msg);
    pTxCharacteristic->setValue((uint8_t *)msg, strlen(msg));
    pTxCharacteristic->notify();
  }
  else if (strncmp(p_token,"si",2)==0) {
    p_token = (char *)strtok(NULL, "=");
    if (p_token) {
      prefs.putString(PREFS_SI, p_token);
      si = p_token;
    }
    sprintf(msg, "si=%s\n", si);
    printf(msg);
    pTxCharacteristic->setValue((uint8_t *)msg, strlen(msg));
    pTxCharacteristic->notify();
  }
  else if (strncmp(p_token,"re",2)==0) {
    ESP.restart();
  }
  else {
    Serial.printf("Unknown Command %s\n", cmd);
  }
}
void setupBT () {
  // Create the BLE Device
  BLEDevice::init("AIR_LIGHT");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loopBT () {
  if (deviceConnected) {
//    pTxCharacteristic->setValue(&txValue, 1);
//    pTxCharacteristic->notify();
//    txValue++;
//    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }
  
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
// ----- Bluetooth Code -----//
