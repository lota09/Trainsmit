/*
 * Trainsmit Project - Main Application
 * Target: ESP32-C6 DevKit
 * Features: WiFi, Seoul CityData API, TFT LCD Display
 */

#define LGFX_USE_V1
#include <ArduinoOTA.h>

#include <LovyanGFX.hpp>

#include "API_Handler.h"



// --- TFT LCD 설정 (LovyanGFX) ---
class LGFX_Config : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;
  lgfx::Panel_ILI9488 _panel_instance;

public:
  LGFX_Config() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.pin_sclk = 0;
      cfg.pin_mosi = 1;
      cfg.pin_miso = 2;
      cfg.pin_dc = 11;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 15;
      cfg.pin_rst = 21;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX_Config tft;

SeoulData currentData;
unsigned long lastUpdate = 0;

void updateDisplay() {
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

  // 한국어 전용 폰트 설정 (LovyanGFX 내장 16픽셀)
  tft.setFont(&lgfx::fonts::efontKR_16);
  tft.setTextSize(1);  // efont는 1배수가 가장 깔끔함

  if (!currentData.valid) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(10, 100);
    tft.println("데이터 로딩 중...");
    tft.endWrite();
    return;
  }

  // 상단 바
  tft.fillRect(0, 0, 480, 40, tft.color565(0, 120, 215));
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(15, 12);
  tft.print(currentData.areaName);

  // 날씨 영역
  int y = 55;
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);  // 32px
  tft.setCursor(15, y);
  tft.printf("%s°C", currentData.curTemp.c_str());

  tft.setTextSize(1);  // 16px
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(140, y + 10);
  tft.printf("(%s / %s)", currentData.minTemp.c_str(), currentData.maxTemp.c_str());

  y += 40;
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(15, y);
  tft.print("날씨: ");
  tft.print(currentData.skyStatus);

  y += 25;
  tft.setCursor(15, y);
  tft.print(currentData.pcpMsg);

  // 미세먼지
  y += 30;
  tft.drawFastHLine(10, y, 460, TFT_DARKGREY);
  y += 10;
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(15, y);
  tft.printf("PM10: %s(%s)", currentData.pm10Idx.c_str(), currentData.pm10.c_str());
  tft.setCursor(240, y);
  tft.printf("PM25: %s(%s)", currentData.pm25Idx.c_str(), currentData.pm25.c_str());

  // 지하철
  y += 40;
  tft.drawFastHLine(10, y, 460, TFT_DARKGREY);
  y += 10;

  for (int i = 0; i < 2; i++) {
    tft.setTextColor(TFT_ORANGE);
    tft.setCursor(15, y);
    tft.printf("[%s]", currentData.subway[i].direction.c_str());

    y += 25;
    tft.setTextColor(TFT_WHITE);
    if (currentData.subway[i].count == 0) {
      tft.setCursor(35, y);
      tft.print("도착 정보 없음");
      y += 25;
    } else {
      for (int j = 0; j < currentData.subway[i].count; j++) {
        tft.setCursor(35, y);
        tft.printf("%s : %d분 %d초",
            currentData.subway[i].arrivals[j].type.c_str(),
            currentData.subway[i].arrivals[j].min,
            currentData.subway[i].arrivals[j].sec);
        y += 25;
      }
    }
    y += 5;
  }

  // 하단 혼잡도
  y = 440;
  tft.drawFastHLine(10, y, 460, TFT_DARKGREY);
  y += 10;
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setCursor(15, y);
  tft.printf("역사 혼잡도: %d명 / 10분", currentData.avgCongestion);

  tft.endWrite();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi 초기화를 가장 먼저 수행 (메모리 부족 방지)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  // TFT 초기화
  tft.init();
  tft.setRotation(5);
  tft.fillScreen(TFT_BLACK);

  ArduinoOTA.setHostname("Trainsmit-Main");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  // 초기 데이터 가져오기
  currentData = fetchSeoulData();
  updateDisplay();
}

void loop() {
  ArduinoOTA.handle();

  if (millis() - lastUpdate > 60000) {  // 1분마다 업데이트 확인 (실제 API 호출은 10분 권장)
    lastUpdate = millis();
    currentData = fetchSeoulData();
    updateDisplay();
  }
}
