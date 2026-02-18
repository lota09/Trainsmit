/*
 * Trainsmit Project - Comprehensive Seoul CityData Test
 * Target: ESP32-C6 DevKit
 *
 * 기능:
 * 1. Wi-Fi 및 OTA 지원
 * 2. required_field.md의 정보를 바탕으로 CityData API 통합 조회
 * 3. 기온, 날씨메시지, 예보, 미세먼지, 지하철 도착 및 혼잡도 출력
 */

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "secrets.h"


// --- 설정 ---
#define CITYDATA_POI "서울식물원·마곡나루역"
const char* seoul_api_base = "http://openapi.seoul.go.kr:8088/";

// URL 인코딩 (한글 POI 대응)
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

void fetchAllSeoulData() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  // Python 스크립트와 동일하게 1/1 인덱스 사용
  String url = String(seoul_api_base) + SEOUL_GENERAL_KEY + "/json/citydata/1/1/" + urlEncode(CITYDATA_POI);

  Serial.println("\n[System] Fetching all data from Seoul CityData API...");
  Serial.printf("[System] URL: %s\n", url.c_str());

  http.useHTTP10(true);
  http.setTimeout(15000);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    Serial.printf("[System] Free heap before parsing: %d bytes\n", ESP.getFreeHeap());

    // 1. 선별적 필터 설정 (유연성 확보)
    StaticJsonDocument<1024> filter;
    filter["RESULT"] = true;
    filter["CITYDATA"]["AREA_NM"] = true;
    filter["CITYDATA"]["WEATHER_STTS"] = true;
    filter["CITYDATA"]["SUB_STTS"] = true;
    filter["CITYDATA"]["LIVE_SUB_PPLTN"] = true;

    // 2. 결과물 바구니 크기
    DynamicJsonDocument doc(24576);
    DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

    if (!error) {
      Serial.printf("[System] Free heap after parsing: %d bytes\n", ESP.getFreeHeap());

      if (!doc.containsKey("CITYDATA")) {
        if (doc.containsKey("RESULT")) {
          Serial.printf(">> API Result Code: %s\n", doc["RESULT"]["RESULT.CODE"] | "unknown");
          Serial.printf(">> API Message: %s\n", doc["RESULT"]["RESULT.MESSAGE"] | "unknown");
        } else {
          Serial.println(">> Error: CITYDATA key missing in response.");
        }
        return;
      }

      JsonObject cityData = doc["CITYDATA"];
      String areaName = cityData["AREA_NM"] | "알 수 없는 지역";

      // --- [날씨] 데이터 추출 (Old version 언래핑 방식) ---
      JsonVariant weather = cityData["WEATHER_STTS"];
      if (weather.containsKey("WEATHER_STTS")) weather = weather["WEATHER_STTS"];
      if (weather.is<JsonArray>()) {
        if (weather.size() > 0)
          weather = weather[0];
        else
          weather = JsonVariant();
      }

      if (weather.isNull()) {
        Serial.println(">> Error: Weather data missing (NULL).");
        return;
      }
      JsonObject weatherStts = weather.as<JsonObject>();

      // --- [날씨] ---
      Serial.println("\n[날씨]");
      String curTemp = weatherStts["TEMP"] | "-";
      String minTemp = weatherStts["MIN_TEMP"] | "-";
      String maxTemp = weatherStts["MAX_TEMP"] | "-";
      Serial.printf("현재 기온 : %s'C (%s'C ~ %s'C)\n", curTemp.c_str(), minTemp.c_str(), maxTemp.c_str());

      String precptType = weatherStts["PRECPT_TYPE"] | "없음";
      String precptAmt = weatherStts["PRECIPITATION"] | "0";

      if (precptType == "없음") {
        JsonArray fcst = weatherStts["FCST24HOURS"];
        String currentSky = (!fcst.isNull() && fcst.size() > 0) ? fcst[0]["SKY_STTS"].as<String>() : "확인불가";
        Serial.printf("날씨 : %s\n", currentSky.c_str());
      } else {
        Serial.printf("날씨 : %s(%smm)\n", precptType.c_str(), precptAmt.c_str());
      }

      String pcpMsg = weatherStts["PCP_MSG"] | "안내 없음";
      Serial.printf("날씨 메시지 : %s\n", pcpMsg.c_str());

      // 날씨 예보 안전 파싱
      JsonArray fcstArray = weatherStts["FCST24HOURS"];
      if (!fcstArray.isNull() && fcstArray.size() > 0) {
        int maxRainChance = -1;
        String fcstTime = "", fcstSky = "";
        for (JsonObject f : fcstArray) {
          int r = f["RAIN_CHANCE"] | 0;
          if (r > maxRainChance) {
            maxRainChance = r;
            fcstTime = f["FCST_DT"].as<String>();
            fcstSky = f["SKY_STTS"].as<String>();
          }
        }
        if (fcstTime.length() >= 10) {
          String fcstHour = fcstTime.substring(8, 10) + "시";
          Serial.printf("날씨 예보 : %s %s (%d%%)\n", fcstHour.c_str(), fcstSky.c_str(), maxRainChance);
        }
      }

      // --- [미세먼지] ---
      Serial.println("\n[미세먼지]");
      String pm10Idx = weatherStts["PM10_INDEX"] | "-";
      String pm10Val = weatherStts["PM10"] | "-";
      String pm25Idx = weatherStts["PM25_INDEX"] | "-";
      String pm25Val = weatherStts["PM25"] | "-";
      Serial.printf("PM10 : %s(%s)\n", pm10Idx.c_str(), pm10Val.c_str());
      Serial.printf("PM25 : %s(%s)\n", pm25Idx.c_str(), pm25Val.c_str());

      // --- [지하철 - 9호선] ---
      Serial.printf("[지하철 - %s (9호선)]\n", areaName.c_str());

      JsonVariant subVar = cityData["SUB_STTS"];
      if (subVar.is<JsonObject>() && subVar.containsKey("SUB_STTS")) subVar = subVar["SUB_STTS"];
      JsonArray subways = subVar.as<JsonArray>();

      if (subways.isNull() || subways.size() == 0) {
        Serial.println(">> 지하철 정보를 찾을 수 없습니다.");
      } else {
        for (JsonObject stn : subways) {
          String line = stn["SUB_STN_LINE"].as<String>();
          // "9" 또는 "9호선" 모두 대응
          if (line.indexOf("9") == -1) continue;

          JsonArray details = stn["SUB_DETAIL"];
          if (details.isNull()) continue;

          const char* dirs[] = {"상행", "하행"};
          for (const char* targetDir : dirs) {
            Serial.printf("[%s]\n", targetDir);
            int count = 0;
            for (JsonObject arrival : details) {
              if (count >= 2) break;
              if (strcmp(arrival["SUB_DIR"] | "", targetDir) == 0) {
                // 문자열을 숫자로 확실하게 변환
                int totalSec = atoi(arrival["SUB_ARVTIME"] | "0");
                const char* routeName = arrival["SUB_ROUTE_NM"] | "";
                bool isExpress = strstr(routeName, "급행") != NULL;

                Serial.printf("%s : %d분 %d초\n",
                    isExpress ? "급행" : "일반",
                    totalSec / 60, totalSec % 60);
                count++;
              }
            }
          }
        }
      }

      // 역사내 혼잡도 평균 계산
      JsonVariant subPopVar = cityData["LIVE_SUB_PPLTN"];
      if (subPopVar.containsKey("LIVE_SUB_PPLTN")) subPopVar = subPopVar["LIVE_SUB_PPLTN"];
      JsonObject subPop = subPopVar.as<JsonObject>();

      if (!subPop.isNull()) {
        int gtonMin = atoi(subPop["SUB_10WTHN_GTON_PPLTN_MIN"] | "0");
        int gtonMax = atoi(subPop["SUB_10WTHN_GTON_PPLTN_MAX"] | "0");
        int gtoffMin = atoi(subPop["SUB_10WTHN_GTOFF_PPLTN_MIN"] | "0");
        int gtoffMax = atoi(subPop["SUB_10WTHN_GTOFF_PPLTN_MAX"] | "0");
        int avg = (gtonMin + gtonMax + gtoffMin + gtoffMax) / 4;

        Serial.printf("\n[승차하인원 평균] : %d명 / 10분\n", avg);
      }

    } else {
      Serial.print("Deserialize Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("HTTP Fail: %d\n", httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // OTA 설정
  ArduinoOTA.setHostname("Trainsmit-C6");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  // 최초 데이터 호출
  fetchAllSeoulData();
}

void loop() {
  ArduinoOTA.handle();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 600000) {  // 10분마다 갱신
    lastUpdate = millis();
    fetchAllSeoulData();
  }
}
