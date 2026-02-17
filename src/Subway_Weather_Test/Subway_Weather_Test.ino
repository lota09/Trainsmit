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

// 환경 데이터 fetch (안전한 우회 전략)
void fetchCityData() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  // 1. 미세먼지 (서울시 RealtimeCityAir - 1/25 전체 구 수신)
  String airUrl = "http://openapi.seoul.go.kr:8088/" + String(SEOUL_GENERAL_KEY) + "/json/RealtimeCityAir/1/25";
  Serial.println("\n[AirQuality] Requesting Seoul Air Data...");
  http.begin(airUrl);
  if (http.GET() == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.printf("Payload length: %d\n", payload.length());
    DynamicJsonDocument doc(10240);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      Serial.println(">> Air Quality JSON parsing failed. Top-level keys:");
      // Attempt to print top-level keys if it's a valid JSON but not the expected structure
      DynamicJsonDocument tempDoc(10240);  // Use a temporary doc for partial parsing if needed
      if (!deserializeJson(tempDoc, payload)) {
        JsonObject root = tempDoc.as<JsonObject>();
        for (JsonPair p : root) {
          Serial.printf("  - %s\n", p.key().c_str());
        }
      }
    } else if (doc.containsKey("RealtimeCityAir")) {
      JsonArray rows = doc["RealtimeCityAir"]["row"];
      bool found = false;
      for (JsonObject row : rows) {
        String loc = row["MSRSTE_NM"].as<String>();
        if (loc.indexOf("강서") >= 0) {
          Serial.printf(">> 미세먼지(%s): %s | 상태: %s\n",
              loc.c_str(), row["PM10"].as<const char*>(), row["IDEX_NM"].as<const char*>());
          found = true;
          break;
        }
      }
      if (!found) Serial.println(">> '강서' 지역을 찾을 수 없습니다.");
    } else {
      Serial.println(">> JSON 구조가 다릅니다. 받은 데이터 앞부분:");
      Serial.println(payload.substring(0, 100));
      Serial.println(">> Top-level keys found:");
      JsonObject root = doc.as<JsonObject>();
      for (JsonPair p : root) {
        Serial.printf("  - %s\n", p.key().c_str());
      }
    }
  }
  http.end();

  // 2. 날씨 (기상청 RSS - 마곡동 기준 1150060300)
  // 별도 API 키 없이 2KB 미만으로 매우 안정적임
  String weatherUrl = "http://www.kma.go.kr/wid/queryDFSRSS.jsp?zone=1150060300";
  Serial.println("[Weather] Requesting KMA RSS (Magok-dong)...");
  http.begin(weatherUrl);
  if (http.GET() == HTTP_CODE_OK) {
    String payload = http.getString();
    // XML 파싱 대신 가벼운 문자열 찾기 사용
    int tempStart = payload.indexOf("<temp>") + 6;
    int tempEnd = payload.indexOf("</temp>");
    int wfStart = payload.indexOf("<wfKor>") + 7;
    int wfEnd = payload.indexOf("</wfKor>");

    if (tempStart > 5 && tempEnd > tempStart && wfStart > 6 && wfEnd > wfStart) {
      String temp = payload.substring(tempStart, tempEnd);
      String sky = payload.substring(wfStart, wfEnd);
      Serial.printf(">> 날씨: %s | 기온: %s도\n", sky.c_str(), temp.c_str());
    } else {
      Serial.println(">> 기상청 데이터를 해석할 수 없습니다.");
      Serial.println(payload.substring(0, min((int)payload.length(), 200)));  // Print beginning of payload for debugging
    }
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
