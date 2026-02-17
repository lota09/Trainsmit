/*
 * Trainsmit Project - TFT LCD Test (LovyanGFX Version)
 * Target: ESP32-C6 DevKit
 * Library: LovyanGFX
 */

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ESP32-C6 DevKit을 위한 LovyanGFX 설정 클래스
class LGFX_Config : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;
  lgfx::Panel_ILI9488 _panel_instance; // 3.5인치 보통 ILI9488 사용

public:
  LGFX_Config() {
    { // SPI 버스 설정
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST; // C6의 FSPI 사용
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // 쓰기 속도 40MHz
      cfg.freq_read = 16000000;
      cfg.pin_sclk = 0; // SCLK
      cfg.pin_mosi = 1; // MOSI
      cfg.pin_miso = 2; // MISO
      cfg.pin_dc = 11;  // DC
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // 패널 설정
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 15;  // CS
      cfg.pin_rst = 21; // RST
      cfg.pin_busy = -1;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX_Config tft; // 설정된 인스턴스 생성

void setup() {
  tft.init();
  tft.setRotation(5); // 1(가로) + 4(반전) = 5 (가로 반전)

  // 배경색 및 텍스트 출력
  tft.fillScreen(TFT_BLACK);

  // 상단 바 (디자인 포인트!)
  tft.fillRect(0, 0, 480, 40, tft.color565(0, 120, 215));

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  tft.println("TRAINSMIT - SYSTEM READY");

  // 메인 메시지
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(4);
  tft.setCursor(50, 100);
  tft.println("LovyanGFX Test");

  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(50, 160);
  tft.println("ESP32-C6 DevKit Success!");

  tft.drawRect(40, 80, 400, 120, TFT_WHITE);
}

void loop() {
  // 작동 시간 표시
  tft.fillRect(50, 220, 200, 30, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(50, 220);
  tft.printf("Uptime: %lu s", millis() / 1000);
  delay(1000);
}