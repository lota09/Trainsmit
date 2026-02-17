/*
 * Trainsmit Project - Subway & Weather Information Test
 * Target: ESP32-C6 DevKit
 *
 * 기능:
 * 1. Wi-Fi 연결
 * 2. 서울시 열린데이터 광장 API를 통한 마곡나루역 실시간 도착 정보 조회
 * 3. 기상청 RSS를 통한 마곡동 동네예보 조회
 */

#include <ArduinoJson.h>  // 라이브러리 관리자에서 설치 필요
#include <ArduinoOTA.h>   // OTA 업데이트를 위한 헤더 추가
#include <HTTPClient.h>
#include <WiFi.h>

#include "secrets.h"  // 비밀 키 관리 파일 포함


// --- 위치 설정 ---
#define TARGET_STATION "마곡나루"
#define CITYDATA_POI   "서울식물원·마곡나루역"  // 정확한 POI 명칭 (특수문자 포함)

// --- API 엔드포인트 ---
const char* seoul_api_url = "http://openapi.seoul.go.kr:8088/";

// 한글 URL 엔코딩 함수 (UTF-8 대응)
String urlEncode(String str) {
  String encodedString = "";
  for (int i = 0; i < str.length(); i++) {
    unsigned char c = (unsigned char)str[i];
    if (isalnum(c))
      encodedString += (char)c;
    else {
      char buf[4];
      sprintf(buf, "%%%02X", c);
      encodedString += buf;
    }
  }
  return encodedString;
}

void fetchSubwayInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(seoul_api_url) + SUBWAY_API_KEY + "/json/realtimeStationArrival/0/5/" + urlEncode(TARGET_STATION);

    Serial.println("\n[Subway] Requesting: " + String(TARGET_STATION));
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(8192);
      deserializeJson(doc, payload);
      if (doc.containsKey("realtimeArrivalList")) {
        JsonArray list = doc["realtimeArrivalList"];
        for (JsonObject arrival : list) {
          Serial.printf(">> %s: %s\n", (const char*)arrival["trainLineNm"], (const char*)arrival["arvlMsg2"]);
        }
      } else {
        Serial.println(">> 도착 정보 없음");
      }
    } else {
      Serial.printf("Subway HTTP Fail: %d\n", httpCode);
    }
    http.end();
  }
}

// 환경 데이터 fetch (CityData API 복원 - Stream Parsing & Filter 적용)
void fetchCityData() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  // CityData API URL (POI 명칭 인코딩 필수)
  String url = String(seoul_api_url) + SEOUL_GENERAL_KEY + "/json/citydata/1/1/" + urlEncode(CITYDATA_POI);

  Serial.println("\n[CityData] Requesting: " + String(CITYDATA_POI));

  // 대용량 데이터 안정성 확보를 위한 HTTP 1.0 설정
  http.useHTTP10(true);
  http.setTimeout(15000);  // 15초 대기
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    // 1. 선별적 파싱을 위한 필터 정의 (메모리 절약 핵심)
    StaticJsonDocument<512> filter;
    filter["CITYDATA"]["WEATHER_STTS"] = true;
    filter["CITYDATA"]["LIVE_PPLTN_STTS"] = true;
    filter["RESULT"] = true;

    // 2. 스트림 파싱 시도
    DynamicJsonDocument doc(8192);  // 필터링된 결과만 담을 공간
    DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

    if (!error) {
      if (doc.containsKey("CITYDATA")) {
        // --- 날씨 데이터 추출 ---
        JsonVariant weather = doc["CITYDATA"]["WEATHER_STTS"];
        // 중첩 구조 해제 (API 특성상 WEATHER_STTS 아래 또 WEATHER_STTS가 있을 수 있음)
        if (weather.containsKey("WEATHER_STTS")) weather = weather["WEATHER_STTS"];
        if (weather.is<JsonArray>()) weather = weather[0];

        if (!weather.isNull()) {
          String temp = weather["TEMP"].as<String>();
          String sky = weather["SKY_STTS"].as<String>();
          String pm10 = weather["PM10"].as<String>();
          String rainMsg = weather["PCP_MSG"].as<String>();

          Serial.printf(">> 날씨: %s | 기온: %s도 | 미세먼지: %s\n", sky.c_str(), temp.c_str(), pm10.c_str());
          Serial.printf(">> 기상안내: %s\n", rainMsg.c_str());
        }

        // --- 인구 혼잡도 데이터 추출 ---
        JsonVariant population = doc["CITYDATA"]["LIVE_PPLTN_STTS"];
        if (population.containsKey("LIVE_PPLTN_STTS")) population = population["LIVE_PPLTN_STTS"];
        if (population.is<JsonArray>()) population = population[0];

        if (!population.isNull()) {
          String congest = population["AREA_CONGEST_LVL"].as<String>();
          String msg = population["AREA_CONGEST_MSG"].as<String>();
          Serial.printf(">> 혼잡도: %s (%s)\n", congest.c_str(), msg.c_str());
        }

      } else if (doc.containsKey("RESULT")) {
        Serial.printf(">> API 오류: %s\n", doc["RESULT"]["MESSAGE"].as<const char*>());
      } else {
        Serial.println(">> CITYDATA 키를 찾을 수 없습니다.");
      }
    } else {
      Serial.print(">> JSON 파싱 에러 (Filter 적용): ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("CityData HTTP Fail: %d\n", httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  ArduinoOTA.setHostname("Trainsmit-C6");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  fetchSubwayInfo();
  fetchCityData();
}

void loop() {
  ArduinoOTA.handle();
  static unsigned long lastMillis = 0;
  if (millis() - lastMillis > 600000) {  // 도시데이터는 10분마다 갱신 권장
    lastMillis = millis();
    fetchSubwayInfo();
    fetchCityData();
  }
}
