#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "SeoulData.h"
#include "secrets.h"


// URL 인코딩 함수
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ')
      encodedString += '+';
    else if (isalnum(c))
      encodedString += c;
    else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

const char* seoul_api_base = "http://openapi.seoul.go.kr:8088/";
const char* CITYDATA_POI = "서울식물원·마곡나루역";

SeoulData fetchSeoulData() {
  SeoulData data;
  data.valid = false;

  if (WiFi.status() != WL_CONNECTED) return data;

  HTTPClient http;
  String url = String(seoul_api_base) + SEOUL_GENERAL_KEY + "/json/citydata/1/1/" + urlEncode(CITYDATA_POI);

  http.useHTTP10(true);
  http.setTimeout(15000);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(32768);
    StaticJsonDocument<512> filter;
    filter["CITYDATA"]["AREA_NM"] = true;
    filter["CITYDATA"]["WEATHER_STTS"][0] = true;
    filter["CITYDATA"]["SUB_STTS"] = true;
    filter["CITYDATA"]["LIVE_SUB_PPLTN"] = true;

    DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

    if (!error) {
      data.valid = true;
      JsonObject cityData = doc["CITYDATA"];
      data.areaName = cityData["AREA_NM"].as<String>();

      // 날씨 데이터 언래핑
      JsonVariant weather = cityData["WEATHER_STTS"];
      if (weather.is<JsonArray>() && weather.size() > 0) weather = weather[0];
      JsonObject weatherStts = weather.as<JsonObject>();

      data.curTemp = weatherStts["TEMP"].as<String>();
      data.minTemp = weatherStts["MIN_TEMP"].as<String>();
      data.maxTemp = weatherStts["MAX_TEMP"].as<String>();
      data.pcpMsg = weatherStts["PCP_MSG"].as<String>();

      JsonArray fcst = weatherStts["FCST24HOURS"];
      data.skyStatus = (!fcst.isNull() && fcst.size() > 0) ? fcst[0]["SKY_STTS"].as<String>() : "확인불가";

      data.pm10 = weatherStts["PM10"].as<String>();
      data.pm10Idx = weatherStts["PM10_INDEX"].as<String>();
      data.pm25 = weatherStts["PM25"].as<String>();
      data.pm25Idx = weatherStts["PM25_INDEX"].as<String>();

      // 지하철
      JsonVariant subVar = cityData["SUB_STTS"];
      if (subVar.is<JsonObject>() && subVar.containsKey("SUB_STTS")) subVar = subVar["SUB_STTS"];
      JsonArray subways = subVar.as<JsonArray>();

      if (!subways.isNull()) {
        for (JsonObject stn : subways) {
          String line = stn["SUB_STN_LINE"].as<String>();
          if (line.indexOf("9") == -1) continue;

          JsonArray details = stn["SUB_DETAIL"];
          const char* targetDirs[] = {"상행", "하행"};
          for (int d = 0; d < 2; d++) {
            data.subway[d].direction = targetDirs[d];
            data.subway[d].count = 0;
            for (JsonObject arrival : details) {
              if (data.subway[d].count >= 2) break;
              if (strcmp(arrival["SUB_DIR"] | "", targetDirs[d]) == 0) {
                int totalSec = atoi(arrival["SUB_ARVTIME"] | "0");
                const char* routeName = arrival["SUB_ROUTE_NM"] | "";
                bool isExpress = strstr(routeName, "급행") != NULL;

                int idx = data.subway[d].count;
                data.subway[d].arrivals[idx].type = isExpress ? "급행" : "일반";
                data.subway[d].arrivals[idx].min = totalSec / 60;
                data.subway[d].arrivals[idx].sec = totalSec % 60;
                data.subway[d].count++;
              }
            }
          }
        }
      }

      // 혼잡도
      JsonVariant subPopVar = cityData["LIVE_SUB_PPLTN"];
      if (subPopVar.is<JsonObject>() && subPopVar.containsKey("LIVE_SUB_PPLTN")) subPopVar = subPopVar["LIVE_SUB_PPLTN"];
      JsonObject subPop = subPopVar.as<JsonObject>();
      if (!subPop.isNull()) {
        int gtonMin = atoi(subPop["SUB_10WTHN_GTON_PPLTN_MIN"] | "0");
        int gtonMax = atoi(subPop["SUB_10WTHN_GTON_PPLTN_MAX"] | "0");
        int gtoffMin = atoi(subPop["SUB_10WTHN_GTOFF_PPLTN_MIN"] | "0");
        int gtoffMax = atoi(subPop["SUB_10WTHN_GTOFF_PPLTN_MAX"] | "0");
        data.avgCongestion = (gtonMin + gtonMax + gtoffMin + gtoffMax) / 4;
      }
    }
  }
  http.end();
  return data;
}

#endif
