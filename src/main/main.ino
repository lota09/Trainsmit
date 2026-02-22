#include <ArduinoOTA.h>
#include <WiFi.h>

#include "API_Handler.h"
#include "TFT_Handler.h"
#include "secrets.h"



SeoulData currentData;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi 초기화
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // WiFi Connected

  // NTP 시간 동기화 (한국 표준시 UTC+9)
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // TFT 초기화 (모듈화된 함수 사용)
  initDisplay();

  ArduinoOTA.setHostname("Trainsmit-Main");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  // 초기 데이터 가져오기 및 표시
  currentData = fetchSeoulData();
  updateDisplay(currentData);
}

void loop() {
  ArduinoOTA.handle();

  if (millis() - lastUpdate > 60000) {  // 1분마다 업데이트 확인
    lastUpdate = millis();
    currentData = fetchSeoulData();
    updateDisplay(currentData);
  }
}
